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
#include <dw1000/dw1000_rng.h>
#include <dw1000/dw1000_ftypes.h>
#include <tdma/dw1000_tdma.h>
#include <ccp/dw1000_ccp.h>

#if MYNEWT_VAL(DW1000_LWIP)
#include <dw1000/dw1000_lwip.h>
#endif
#if MYNEWT_VAL(DW1000_PAN)
#include <dw1000/dw1000_pan.h>
#endif


//#define DIAGMSG(s,u) printf(s,u)
#ifndef DIAGMSG
#define DIAGMSG(s,u)
#endif

static dw1000_rng_config_t rng_config = {
    .tx_holdoff_delay = 0x0300,      // Send Time delay in usec.
    .rx_timeout_period = 0x1        // Receive response timeout in usec
};

#if MYNEWT_VAL(DW1000_PAN)
static dw1000_pan_config_t pan_config = {
    .tx_holdoff_delay = 0x0C00,         // Send Time delay in usec.
    .rx_timeout_period = 0x8000         // Receive response timeout in usec.
};
#endif

static twr_frame_t twr[] = {
    [0] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                 // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID
    },
    [1] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                 // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID
    },
    [2] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                 // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID
    },
    [3] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                 // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID
    }
};


#define NSLOTS MYNEWT_VAL(TDMA_NSLOTS)

static uint16_t g_slot[NSLOTS] = {0};

static bool error_cb(struct _dw1000_dev_instance_t * inst);

/*! 
 * @fn slot_timer_cb(struct os_event * ev)
 *
 * @brief This function each 
 *
 * input parameters
 * @param inst - struct os_event *  
 *
 * output parameters
 *
 * returns none 
 */
static void 
slot_timer_cb(struct os_event *ev){
    assert(ev);

    tdma_slot_t * slot = (tdma_slot_t *) ev->ev_arg;
    tdma_instance_t * tdma = slot->parent;
    dw1000_dev_instance_t * inst = tdma->parent;
    dw1000_ccp_instance_t * ccp = inst->ccp;
    uint16_t idx = slot->idx;

    hal_gpio_toggle(LED_BLINK_PIN);
    
#if MYNEWT_VAL(CLOCK_CALIBRATION_ENABLED)
    clkcal_instance_t * clk = ccp->clkcal;
    uint64_t dx_time = (ccp->epoch + (uint64_t) roundf(clk->skew * (double)((idx * (uint64_t)tdma->period << 16)/tdma->nslots)));
#else
    uint64_t dx_time = (ccp->epoch + (uint64_t) (idx * ((uint64_t)tdma->period << 16)/tdma->nslots));
#endif
    dx_time = dx_time & 0xFFFFFFFE00UL;

//    uint32_t tic = os_cputime_ticks_to_usecs(os_cputime_get32());
    if(dw1000_rng_request_delay_start(inst, 0x4321, dx_time, DWT_SS_TWR).start_tx_error){
        uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
        printf("{\"utime\": %lu,\"msg\": \"slot_timer_cb_%d:start_tx_error\",\"%s\":%d}\n",utime,idx,__FILE__, __LINE__); 
    }else{
 //       uint32_t toc = os_cputime_ticks_to_usecs(os_cputime_get32());
 //       printf("{\"utime\": %lu,\"slot_timer_cb_tic_toc\": %lu}\n",toc,toc-tic);
    }

#ifdef VERBOSE
        uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
        printf("{\"utime\": %lu,\"slot\": %d, \"dx_time\": %lX%08lX, \"epoch\": %lX%08lX}\n",utime, idx, 
        (uint32_t)(dx_time >> 32),(uint32_t)(dx_time & 0xFFFFFFFFUL),
        (uint32_t)(ccp->epoch  >> 32),(uint32_t)(ccp->epoch & 0xFFFFFFFFUL)
        );
#endif
}

/*! 
 * @fn slot0_timer_cb(struct os_event * ev)
 * @brief This function is a place holder
 *
 * input parameters
 * @param inst - struct os_event *  
 * output parameters
 * returns none 
 */
static void 
slot0_timer_cb(struct os_event *ev){
  //  printf("{\"utime\": %lu,\"msg\": \"%s:[%d]:slot0_timer_cb\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()),__FILE__, __LINE__); 
}

/*! 
 * @fn frame_complete_cb(struct os_event * ev)
 *
 * @brief This function each 
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
    
    twr_frame_t * frame = rng->frames[(rng->idx)%rng->nframes];
 
    if (frame->code == DWT_SS_TWR_FINAL) {
        float time_of_flight = (float) dw1000_rng_twr_to_tof(rng);
        float range = dw1000_rng_tof_to_meters(dw1000_rng_twr_to_tof(rng));
        printf("{\"utime\": %lu,\"tof\": %lu,\"range\": %lu,\"res_req\": \"%lX\","
                " \"rec_tra\": \"%lX\"}\n",
                os_cputime_ticks_to_usecs(os_cputime_get32()),
                *(uint32_t *)(&time_of_flight), 
                *(uint32_t *)(&range),
                (frame->response_timestamp - frame->request_timestamp),
                (frame->transmission_timestamp - frame->reception_timestamp)
        );
        frame->code = DWT_SS_TWR_END;
    }

    else if (frame->code == DWT_DS_TWR_FINAL) {
        float time_of_flight = dw1000_rng_twr_to_tof(rng);
        float range = dw1000_rng_tof_to_meters(dw1000_rng_twr_to_tof(rng));
        printf("{\"utime\": %lu,\"tof\": %lu,\"range\": %lu,\"azimuth\": %lu,\"res_req\":\"%lX\","
                " \"rec_tra\": \"%lX\"}\n",
                os_cputime_ticks_to_usecs(os_cputime_get32()), 
                *(uint32_t *)(&time_of_flight), 
                *(uint32_t *)(&range),
                *(uint32_t *)(&frame->spherical.azimuth),
                (frame->response_timestamp - frame->request_timestamp),
                (frame->transmission_timestamp - frame->reception_timestamp)
        );
        frame->code = DWT_DS_TWR_END;
    } 

    else if (frame->code == DWT_DS_TWR_EXT_FINAL) {
        float time_of_flight = dw1000_rng_twr_to_tof(rng);
        printf("{\"utime\": %lu,\"tof\": %lu,\"range\": %lu,\"azimuth\": %lu,\"res_req\":\"%lX\","
                " \"rec_tra\": \"%lX\"}\n",
                os_cputime_ticks_to_usecs(os_cputime_get32()), 
                *(uint32_t *)(&time_of_flight), 
                *(uint32_t *)(&frame->spherical.range),
                *(uint32_t *)(&frame->spherical.azimuth),
                (frame->response_timestamp - frame->request_timestamp),
                (frame->transmission_timestamp - frame->reception_timestamp)
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
error_cb(struct _dw1000_dev_instance_t * inst) {
    if(inst->fctrl != FCNTL_IEEE_RANGE_16){
        return false;
    }   

    return true;
    uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
    if (inst->status.start_rx_error)
        printf("{\"utime\": %lu,\"msg\": \"start_rx_error,%s:%d\"}\n",utime, __FILE__, __LINE__);
    if (inst->status.start_tx_error)
        printf("{\"utime\": %lu,\"msg\": \"start_tx_error,%s:%d\"}\n",utime, __FILE__, __LINE__);
    if (inst->status.rx_error)
        printf("{\"utime\": %lu,\"msg\": \"rx_error\",%s:%d\"}\n",utime, __FILE__, __LINE__);

    return true;
}



/*! 
 * @fn rx_complete_cb(dw1000_dev_instance_t * inst)
 *
 * @brief This callback is in the interrupt context and is uses to schedule an rx_complete event on the default event queue.  
 * Processing should be kept to a minimum giving the context. All algorithms should be deferred to a thread on an event queue. 
 * In this example all postprocessing is performed in the pdoa_ev_cb.
 * input parameters
 * @param inst - dw1000_dev_instance_t * 
 *
 * output parameters
 *
 * returns none 
 */
/* The timer callout */
static struct os_callout slot_complete_callout;

static bool 
rx_complete_cb(struct _dw1000_dev_instance_t * inst){
    if(inst->fctrl != FCNTL_IEEE_RANGE_16){
        return false;
    }
    os_callout_init(&slot_complete_callout, os_eventq_dflt_get(), slot_complete_cb, inst);
    os_eventq_put(os_eventq_dflt_get(), &slot_complete_callout.c_ev);
    return false;
}


int main(int argc, char **argv){
    int rc;

    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);
    
    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);

    inst->PANID = 0xDECA;
    inst->my_short_address = MYNEWT_VAL(DEVICE_ID);
    inst->my_long_address = ((uint64_t) inst->device_id << 32) + inst->partID;

    dw1000_set_panid(inst,inst->PANID);
    dw1000_mac_init(inst, &inst->config);
    dw1000_rng_init(inst, &rng_config, sizeof(twr)/sizeof(twr_frame_t));
    dw1000_rng_set_frames(inst, twr, sizeof(twr)/sizeof(twr_frame_t));
    
    dw1000_extension_callbacks_t cbs = {
        .tx_error_cb = error_cb,
        .rx_error_cb = error_cb,
        .rx_complete_cb = rx_complete_cb
    };
    dw1000_add_extension_callbacks(inst, cbs);

#if MYNEWT_VAL(DW1000_CCP_ENABLED)
    dw1000_ccp_init(inst, 2, MYNEWT_VAL(UUID_CCP_MASTER));
    dw1000_ccp_start(inst, CCP_ROLE_SLAVE);
#endif
#if MYNEWT_VAL(DW1000_PAN)
    dw1000_pan_init(inst, &pan_config);   
    dw1000_pan_start(inst, DWT_NONBLOCKING);  
#endif

    printf("device_id = 0x%lX\n",inst->device_id);
    printf("PANID = 0x%X\n",inst->PANID);
    printf("DeviceID = 0x%X\n",inst->my_short_address);
    printf("partID = 0x%lX\n",inst->partID);
    printf("lotID = 0x%lX\n",inst->lotID);
    printf("xtal_trim = 0x%X\n",inst->xtal_trim);  
    printf("frame_duration = %d usec\n",dw1000_phy_frame_duration(&inst->attrib, sizeof(twr_frame_final_t))); 
    printf("SHR_duration = %d usec\n",dw1000_phy_SHR_duration(&inst->attrib)); 
    printf("holdoff = %d usec\n",(uint16_t)ceilf(dw1000_dwt_usecs_to_usecs(rng_config.tx_holdoff_delay))); 

   for (uint16_t i = 0; i < sizeof(g_slot)/sizeof(uint16_t); i++)
        g_slot[i] = i;
    tdma_init(inst, MYNEWT_VAL(TDMA_PERIOD), NSLOTS); 
    tdma_assign_slot(inst->tdma, slot0_timer_cb, g_slot[0], &g_slot[0]);
    for (uint16_t i = 1; i < sizeof(g_slot)/sizeof(uint16_t); i++)
        tdma_assign_slot(inst->tdma, slot_timer_cb, g_slot[i], &g_slot[i]);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}

