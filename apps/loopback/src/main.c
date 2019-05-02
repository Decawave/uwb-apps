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

#include <dw1000/dw1000_dev.h>
#include <dw1000/dw1000_hal.h>
#include <dw1000/dw1000_phy.h>
#include <dw1000/dw1000_mac.h>
#include <dw1000/dw1000_ftypes.h>

#if MYNEWT_VAL(RNG_ENABLED)
#include <rng/rng.h>
#endif
#if MYNEWT_VAL(TWR_DS_EXT_ENABLED)
#include <twr_ds_ext/twr_ds_ext.h>
#endif
#if MYNEWT_VAL(TDMA_ENABLED)
#include <tdma/tdma.h>
#endif
#if MYNEWT_VAL(CCP_ENABLED)
#include <ccp/ccp.h>
#endif
#if MYNEWT_VAL(WCS_ENABLED)
#include <wcs/wcs.h>
#endif
#if MYNEWT_VAL(DW1000_LWIP)
#include <lwip/lwip.h>
#endif
#if MYNEWT_VAL(TIMESCALE)
#include <timescale/timescale.h> 
#endif

static void master_slot_ev_cb(struct os_event * ev);
static bool master_complete_cb(dw1000_dev_instance_t * inst, dw1000_mac_interface_t * cbs);
static void master_complete_ev_cb(struct os_event *ev);

static void slave_slot_ev_cb(struct os_event * ev);
static bool slave_complete_cb(dw1000_dev_instance_t * inst, dw1000_mac_interface_t * cbs);
static void slave_complete_ev_cb(struct os_event *ev);

static uint16_t g_slot[MYNEWT_VAL(TDMA_NSLOTS)] = {0};
#define NINST 2


static void 
clk_sync(dw1000_dev_instance_t * inst[], uint8_t n){

    for (uint8_t i = 0; i < n; i++ )
        dw1000_phy_external_sync(inst[i],33, true);

    hal_gpio_init_out(MYNEWT_VAL(DW1000_PDOA_SYNC_EN), 1);
    hal_gpio_init_out(MYNEWT_VAL(DW1000_PDOA_SYNC_CLR), 1);

    hal_gpio_init_out(MYNEWT_VAL(DW1000_PDOA_SYNC), 1);
    os_cputime_delay_usecs(1000);
    hal_gpio_write(MYNEWT_VAL(DW1000_PDOA_SYNC), 0);

    hal_gpio_write(MYNEWT_VAL(DW1000_PDOA_SYNC_CLR), 0);
    hal_gpio_write(MYNEWT_VAL(DW1000_PDOA_SYNC_EN), 0);
    
    for (uint8_t i = 0; i < n; i++ )
        dw1000_phy_external_sync(inst[i],0, false);
}
   
static void 
master_slot_ev_cb(struct os_event * ev){
    assert(ev);

    tdma_slot_t * slot = (tdma_slot_t *) ev->ev_arg;
    tdma_instance_t * tdma = slot->parent;
    dw1000_dev_instance_t * inst = tdma->parent;
    dw1000_ccp_instance_t * ccp = inst->ccp;
    uint16_t idx = slot->idx;

    dw1000_set_delay_start(inst, tdma_rx_slot_start(inst, idx));
    uint16_t timeout = dw1000_phy_frame_duration(&inst->attrib, sizeof(ieee_rng_response_frame_t))                 
                            + inst->rng->config.tx_holdoff_delay;         // Remote side turn arroud time. 
                            
    dw1000_set_rx_timeout(inst, timeout);
    dw1000_rng_listen(inst, DWT_BLOCKING);
}


static bool
master_complete_cb(dw1000_dev_instance_t * inst, dw1000_mac_interface_t * cbs){
    if(inst->fctrl != FCNTL_IEEE_RANGE_16){
        return false;
    }
    static struct os_callout callout;
    os_callout_init(&callout, os_eventq_dflt_get(), master_complete_ev_cb, inst);
    os_eventq_put(os_eventq_dflt_get(), &callout.c_ev);
    return true;
}


static void
master_complete_ev_cb(struct os_event *ev) {
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);
    
    hal_gpio_toggle(LED_BLINK_PIN);
    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *) ev->ev_arg;
    dw1000_rng_instance_t * rng = inst->rng;
    uint16_t idx = (rng->idx)%rng->nframes;
    twr_frame_t * frame = rng->frames[idx];

    if (frame->code == DWT_SS_TWR_FINAL) {

        float time_of_flight = dw1000_rng_twr_to_tof(rng, idx);
        float range = dw1000_rng_tof_to_meters(time_of_flight);
        float skew = dw1000_calc_clock_offset_ratio(inst, frame->carrier_integrator);
        
        uint32_t utime =os_cputime_ticks_to_usecs(os_cputime_get32()); 
 
        printf("{\"utime\": %lu,\"tof_m\": %lu,\"range\": %lu,\"res_req\": \"%lX\","
                    "\"tra_rec\": \"%lX\", \"skew\": %lu}\n",
                utime,
                *(uint32_t *)(&time_of_flight), 
                *(uint32_t *)(&range),
                (frame->response_timestamp - frame->request_timestamp),
                (frame->transmission_timestamp - frame->reception_timestamp),
                *(uint32_t *)(&skew)
        );
        frame->code = DWT_SS_TWR_END;
    }
}


static void 
slave_slot_ev_cb(struct os_event *ev){
    assert(ev);

    tdma_slot_t * slot = (tdma_slot_t *) ev->ev_arg;
    tdma_instance_t * tdma = slot->parent;
    dw1000_dev_instance_t * inst = tdma->parent;
    dw1000_ccp_instance_t * ccp = inst->ccp;
    uint16_t idx = slot->idx;
    
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

static bool
slave_complete_cb(dw1000_dev_instance_t * inst, dw1000_mac_interface_t * cbs){
    if(inst->fctrl != FCNTL_IEEE_RANGE_16){
        return false;
    }
    static struct os_callout callout;
    os_callout_init(&callout, os_eventq_dflt_get(), slave_complete_ev_cb, inst);
    os_eventq_put(os_eventq_dflt_get(), &callout.c_ev);
    return true;
}

static void
slave_complete_ev_cb(struct os_event *ev) {
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);
    
    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *) ev->ev_arg;
    dw1000_rng_instance_t * rng = inst->rng;
    uint16_t idx = (rng->idx)%rng->nframes;
    twr_frame_t * frame = rng->frames[idx];
    
    float time_of_flight = dw1000_rng_twr_to_tof(rng, idx);
    float range = dw1000_rng_tof_to_meters(time_of_flight);
    float skew = dw1000_calc_clock_offset_ratio(inst, frame->carrier_integrator);
    
    if (frame->code == DWT_SS_TWR_FINAL) {
        printf("{\"utime\": %lu,\"tof_s\": %lu,\"range\": %lu,\"res_req\": \"%lX\","
                "\"tra_rec\": \"%lX\", \"skew\": %lu}\n",
                os_cputime_ticks_to_usecs(os_cputime_get32()),
                *(uint32_t *)(&time_of_flight), 
                *(uint32_t *)(&range),
                (frame->response_timestamp - frame->request_timestamp),
                (frame->transmission_timestamp - frame->reception_timestamp),
                *(uint32_t *)(&skew)
        );
        frame->code = DWT_SS_TWR_END;
    }
}



int main(int argc, char **argv){
    int rc;
    
    dw1000_dev_instance_t * inst[] = {
        hal_dw1000_inst(0),
        hal_dw1000_inst(1)
    };

    inst[0]->config.txrf = inst[1]->config.txrf = (struct _dw1000_dev_txrf_config_t ){
                    .PGdly = TC_PGDELAY_CH5,
                    .BOOSTNORM = dw1000_power_value(DW1000_txrf_config_0db, 0.5),
                    .BOOSTP500 = dw1000_power_value(DW1000_txrf_config_0db, 0.5),
                    .BOOSTP250 = dw1000_power_value(DW1000_txrf_config_0db, 0.5),
                    .BOOSTP125 = dw1000_power_value(DW1000_txrf_config_0db, 0.5)   
                };
    sysinit();

    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);

    uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
    printf("{\"utime\": %lu,\"msg\": \"request_duration = %d usec\"}\n", utime, dw1000_phy_frame_duration(&inst[0]->attrib, sizeof(ieee_rng_request_frame_t) )); 
    printf("{\"utime\": %lu,\"msg\": \"response_duration = %d usec\"}\n",utime, dw1000_phy_frame_duration(&inst[0]->attrib, sizeof(ieee_rng_response_frame_t) )); 
    printf("{\"utime\": %lu,\"msg\": \"SHR_duration = %d usec\"}\n",utime, dw1000_phy_SHR_duration(&inst[0]->attrib)); 
    printf("{\"utime\": %lu,\"msg\": \"holdoff = %d usec\"}\n",utime, (uint16_t)ceilf(dw1000_dwt_usecs_to_usecs(inst[0]->rng->config.tx_holdoff_delay) ));
    
    dw1000_mac_interface_t cbs[] = {
        [0] ={
            .id =  DW1000_APP0,
            .complete_cb = master_complete_cb
        },
        [1] ={
            .id =  DW1000_APP0,
            .complete_cb = slave_complete_cb
        }
    };
    dw1000_mac_append_interface(inst[0], &cbs[0]);
    dw1000_mac_append_interface(inst[1], &cbs[1]);
    dw1000_ccp_start(inst[0], CCP_ROLE_MASTER);
    dw1000_ccp_start(inst[1], CCP_ROLE_SLAVE);

    for (uint16_t i = 0; i < sizeof(g_slot)/sizeof(uint16_t); i++)
        g_slot[i] = i;
    for (uint16_t i = 1; i < sizeof(g_slot)/sizeof(uint16_t); i++){
        tdma_assign_slot(inst[0]->tdma, master_slot_ev_cb, g_slot[i], &g_slot[i]);
        tdma_assign_slot(inst[1]->tdma, slave_slot_ev_cb, g_slot[i], &g_slot[i]);        
    }
    
    clk_sync(inst, 2);
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}

