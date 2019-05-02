/**
 * Copyright (C) 2017-2018, Decawave Limited, All Rights Reserved
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "sysinit/sysinit.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_bsp.h"
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif

#include <dw1000/dw1000_dev.h>
#include <dw1000/dw1000_hal.h>
#include <dw1000/dw1000_phy.h>
#include <dw1000/dw1000_mac.h>
#include <dw1000/dw1000_ftypes.h>
#include <rng/rng.h>
#include <tdma/tdma.h>

#if MYNEWT_VAL(CCP_ENABLED)
#include <ccp/ccp.h>
#endif
#if MYNEWT_VAL(WCS_ENABLED)
#include <wcs/wcs.h>
#endif
#if MYNEWT_VAL(DW1000_LWIP)
#include <lwip/lwip.h>
#endif

//#define DIAGMSG(s,u) printf(s,u)
#ifndef DIAGMSG
#define DIAGMSG(s,u)
#endif

#ifndef TICTOC
#undef TICTOC 
#endif 

static uint16_t g_slot[MYNEWT_VAL(TDMA_NSLOTS)] = {0};
static bool error_cb(dw1000_dev_instance_t * inst, dw1000_mac_interface_t * cbs);
static void slot_complete_cb(struct os_event * ev);
void prph_init(char *);

/*! 
 * @fn slot_cb(struct os_event * ev)
 *
 * @brief In this example slot_cb is used to initiate a range request. The slot_cb is scheduled 
 * MYNEWT_VAL(OS_LATENCY) in advance of the transmit epoch and a delayed start request is issued in advance of 
 * the required epoch. The transmission timing is controlled precisely by the DW1000 with the transmission time 
 * defined by the value of the dw_time variable. If the OS_LATENCY value is set too small the range request 
 * function will report a start_tx_error. In a synchronized network, the node device switches the transceiver 
 * to receiver mode for the same epoch; and will either receive the inbound frame or timeout after the frame 
 * duration as elapsed. This ensures that the transceiver is in receive mode for the minimum time required.   
 *
 * input parameters
 * @param inst - struct os_event *  
 *
 * output parameters
 *
 * returns none 
 */
static void 
slot_cb(struct os_event *ev){
    assert(ev);

    tdma_slot_t * slot = (tdma_slot_t *) ev->ev_arg;
    tdma_instance_t * tdma = slot->parent;
    dw1000_dev_instance_t * inst = tdma->parent;
    dw1000_ccp_instance_t * ccp = inst->ccp;
    uint16_t idx = slot->idx;

    hal_gpio_toggle(LED_BLINK_PIN);  
    
    uint64_t dx_time = tdma_tx_slot_start(inst, idx);
    dx_time = dx_time & 0xFFFFFFFFFE00UL;
  
#ifdef TICTOC
    uint32_t tic = os_cputime_ticks_to_usecs(os_cputime_get32());
#endif
    if(dw1000_rng_request_delay_start(inst, 0x4321, dx_time, DWT_SS_TWR).start_tx_error){
    }else{
#ifdef TICTOC
        os_error_t err = os_sem_pend(&inst->rng->sem, OS_TIMEOUT_NEVER); // Wait for completion of transactions 
        assert(err == OS_OK);
        err = os_sem_release(&inst->rng->sem);
        assert(err == OS_OK);
        uint32_t toc = os_cputime_ticks_to_usecs(os_cputime_get32());
        printf("{\"utime\": %lu,\"dw1000_rng_request_delay_start_tic_toc\": %ld}\n",toc, (toc - tic));
#endif
    }
}

/*! 
 * @fn slot0_cb(struct os_event * ev)
 * @brief This function is a place holder for PAN or other Slot0 related housekeeping.
 *
 * input parameters
 * @param inst - struct os_event *  
 * output parameters
 * returns none 
 */
static void 
slot0_cb(struct os_event *ev){
  //  printf("{\"utime\": %lu,\"msg\": \"%s:[%d]:slot0_timer_cb\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()),__FILE__, __LINE__); 
}


/*! 
 * @fn complete_cb(dw1000_dev_instance_t * inst, dw1000_mac_interface_t * cbs)
 *
 * @brief This callback is part of the  dw1000_mac_interface_t extension interface and invoked of the completion of a range request 
 * in the context of this example. The dw1000_mac_interface_t is in the interrupt context and is used to schedule events an event queue. 
 * Processing should be kept to a minimum giving the interrupt context. All algorithms activities should be deferred to a thread on an event queue. 
 * The callback should return true if and only if it can determine if it is the sole recipient of this event. 
 * 
 * NOTE: The MAC extension interface is a link-list of callbacks, subsequent callbacks on the list will be not be called in the 
 * event of returning true. 
 *
 * @param inst  - dw1000_dev_instance_t *
 * @param cbs   - dw1000_mac_interface_t *
 *
 * output parameters
 *
 * returns bool
 */
/* The timer callout */
static struct os_callout slot_callout;
static uint16_t g_idx_latest;

static bool
complete_cb(dw1000_dev_instance_t * inst, dw1000_mac_interface_t * cbs){
    if(inst->fctrl != FCNTL_IEEE_RANGE_16){
        return false;
    }
    dw1000_rng_instance_t * rng = inst->rng;
    g_idx_latest = (rng->idx)%rng->nframes; // Store valid frame pointer

    os_callout_init(&slot_callout, os_eventq_dflt_get(), slot_complete_cb, inst);
    os_eventq_put(os_eventq_dflt_get(), &slot_callout.c_ev);
    return true;
}


/*! 
 * @fn slot_complete_cb(struct os_event * ev)
 *
 * @brief In the example this function represents the event context processing of the received range request. 
 * In this case, a JSON string is constructed and written to stdio. See the ./apps/matlab or ./apps/python folders for examples on 
 * how to parse and render these results. 
 * 
 * input parameters
 * @param inst - struct os_event *  
 * output parameters
 * returns none 
 */
static void 
slot_complete_cb(struct os_event * ev){
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);

    hal_gpio_toggle(LED_BLINK_PIN);
    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *) ev->ev_arg;
    dw1000_rng_instance_t * rng = inst->rng;
    
    twr_frame_t * frame = rng->frames[g_idx_latest%rng->nframes];
    float skew = dw1000_calc_clock_offset_ratio(inst, frame->carrier_integrator);
    
    if (frame->code == DWT_SS_TWR_FINAL) {
        float time_of_flight = (float) dw1000_rng_twr_to_tof(rng, g_idx_latest);
        float range = dw1000_rng_tof_to_meters(time_of_flight);
        printf("{\"utime\": %lu,\"tof\": %lu,\"range\": %lu,\"res_req\": \"%lX\","
                " \"rec_tra\": \"%lX\", \"skew\": %lu}\n",
                os_cputime_ticks_to_usecs(os_cputime_get32()),
                *(uint32_t *)(&time_of_flight), 
                *(uint32_t *)(&range),
                (frame->response_timestamp - frame->request_timestamp),
                (frame->transmission_timestamp - frame->reception_timestamp),
                *(uint32_t *)(&skew)
        );
        frame->code = DWT_SS_TWR_END;
    }

    else if (frame->code == DWT_DS_TWR_FINAL) {
        float time_of_flight = dw1000_rng_twr_to_tof(rng, g_idx_latest);
        float range = dw1000_rng_tof_to_meters(time_of_flight);
        printf("{\"utime\": %lu,\"tof\": %lu,\"range\": %lu,\"azimuth\": %lu,\"res_req\":\"%lX\","
                " \"rec_tra\": \"%lX\", \"skew\": %lu}\n",
                os_cputime_ticks_to_usecs(os_cputime_get32()), 
                *(uint32_t *)(&time_of_flight), 
                *(uint32_t *)(&range),
                *(uint32_t *)(&frame->spherical.azimuth),
                (frame->response_timestamp - frame->request_timestamp),
                (frame->transmission_timestamp - frame->reception_timestamp),
                *(uint32_t *)(&skew)
        );
        frame->code = DWT_DS_TWR_END;
    } 

    else if (frame->code == DWT_DS_TWR_EXT_FINAL) {
        float time_of_flight = dw1000_rng_twr_to_tof(rng, g_idx_latest);
        printf("{\"utime\": %lu,\"tof\": %lu,\"range\": %lu,\"azimuth\": %lu,\"res_req\":\"%lX\","
                " \"rec_tra\": \"%lX\", \"skew\": %lu}\n",
                os_cputime_ticks_to_usecs(os_cputime_get32()), 
                *(uint32_t *)(&time_of_flight), 
                *(uint32_t *)(&frame->spherical.range),
                *(uint32_t *)(&frame->spherical.azimuth),
                (frame->response_timestamp - frame->request_timestamp),
                (frame->transmission_timestamp - frame->reception_timestamp),
                *(uint32_t *)(&skew)
        );
        frame->code = DWT_DS_TWR_END;
    } 
}


/*! 
 * @fn error_cb(struct os_event *ev)
 *
 * @brief This callback is in the interrupt context and is called on error event.
 * In this example just log event. 
 * Note: interrupt context so overlapping IO is possible
 * input parameters
 * @param inst - dw1000_dev_instance_t * inst
 *
 * output parameters
 *
 * returns none 
 */
static bool
error_cb(dw1000_dev_instance_t * inst, dw1000_mac_interface_t * cbs)
{ 
    if(inst->fctrl != FCNTL_IEEE_RANGE_16){
        return false;
    }   

    uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
    if (inst->status.start_rx_error)
        printf("{\"utime\": %lu,\"msg\": \"start_rx_error,%s:%d\"}\n",utime, __FILE__, __LINE__);
    if (inst->status.start_tx_error)
        printf("{\"utime\": %lu,\"msg\": \"start_tx_error,%s:%d\"}\n",utime, __FILE__, __LINE__);
    if (inst->status.rx_error)
        printf("{\"utime\": %lu,\"msg\": \"rx_error\",%s:%d\"}\n",utime, __FILE__, __LINE__);

    return true;
}

int main(int argc, char **argv){
    int rc;

    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);
    
    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
    char name[32]={0};
    sprintf(name,"%X-%04X",inst->PANID,inst->my_short_address);
    prph_init(name);
    dw1000_mac_interface_t cbs = {
        .id = DW1000_APP0,
        .tx_error_cb = error_cb,
        .rx_error_cb = error_cb,
        .complete_cb = complete_cb
    };
    dw1000_mac_append_interface(inst, &cbs);
    
#if MYNEWT_VAL(CCP_ENABLED)
    dw1000_ccp_start(inst, CCP_ROLE_SLAVE);
#endif

    uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
    printf("{\"utime\": %lu,\"exec\": \"%s\"}\n",utime,__FILE__); 
    printf("{\"utime\": %lu,\"msg\": \"device_id = 0x%lX\"}\n",utime,inst->device_id);
    printf("{\"utime\": %lu,\"msg\": \"PANID = 0x%X\"}\n",utime,inst->PANID);
    printf("{\"utime\": %lu,\"msg\": \"DeviceID = 0x%X\"}\n",utime,inst->my_short_address);
    printf("{\"utime\": %lu,\"msg\": \"partID = 0x%lX\"}\n",utime,inst->partID);
    printf("{\"utime\": %lu,\"msg\": \"lotID = 0x%lX\"}\n",utime,inst->lotID);
    printf("{\"utime\": %lu,\"msg\": \"xtal_trim = 0x%X\"}\n",utime,inst->xtal_trim);  
    printf("{\"utime\": %lu,\"msg\": \"frame_duration = %d usec\"}\n",utime,dw1000_phy_frame_duration(&inst->attrib, sizeof(twr_frame_final_t))); 
    printf("{\"utime\": %lu,\"msg\": \"SHR_duration = %d usec\"}\n",utime,dw1000_phy_SHR_duration(&inst->attrib)); 
    printf("{\"utime\": %lu,\"msg\": \"holdoff = %d usec\"}\n",utime,(uint16_t)ceilf(dw1000_dwt_usecs_to_usecs(inst->rng->config.tx_holdoff_delay))); 

#ifdef TICTOC
    twr_frame_t frame ={0};
    {
        uint32_t tic = os_cputime_ticks_to_usecs(os_cputime_get32());
        dw1000_write_tx(inst, &frame.array[0], 0, sizeof(ieee_rng_request_frame_t));
        os_error_t err = os_sem_pend(&inst->spi_nb_sem, OS_TIMEOUT_NEVER);
        assert(err == OS_OK);
        err = os_sem_release(&inst->spi_nb_sem);
        assert(err == OS_OK);
        uint32_t toc = os_cputime_ticks_to_usecs(os_cputime_get32());
        printf("{\"utime\": %lu,\"msg\": \"dw1000_write_tx(sizeof(ieee_rng_request_frame_t)) = %ld usec\"}\n",toc, (toc - tic));
    }
    {
        uint32_t tic = os_cputime_ticks_to_usecs(os_cputime_get32());
        dw1000_read_rx(inst, &frame.array[0], 0, sizeof(ieee_rng_response_frame_t));
        uint32_t toc = os_cputime_ticks_to_usecs(os_cputime_get32());
        printf("{\"utime\": %lu,\"msg\": \"dw1000_read_rx(sizeof(ieee_rng_response_frame_t)) = %ld usec\"}\n",toc, (toc - tic));
    }
#endif

   for (uint16_t i = 0; i < sizeof(g_slot)/sizeof(uint16_t); i++)
        g_slot[i] = i;
    tdma_assign_slot(inst->tdma, slot0_cb, g_slot[0], &g_slot[0]);
    for (uint16_t i = 2; i < sizeof(g_slot)/sizeof(uint16_t); i+=4)
        tdma_assign_slot(inst->tdma, slot_cb, g_slot[i], &g_slot[i]);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}

