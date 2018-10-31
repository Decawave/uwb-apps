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
#if MYNEWT_VAL(TDMA_ENABLED)
#include <tdma/tdma.h>
#endif
#if MYNEWT_VAL(CCP_ENABLED)
#include <ccp/ccp.h>
#endif
#if MYNEWT_VAL(WCS_ENABLED)
#include <wcs/wcs.h>
#endif
#if MYNEWT_VAL(NRNG_ENABLED)
#include <nranges/nranges.h>
#endif
//#define DIAGMSG(s,u) printf(s,u)
#ifndef DIAGMSG
#define DIAGMSG(s,u)
#endif

#define NSLOTS MYNEWT_VAL(TDMA_NSLOTS)

static uint16_t g_slot[NSLOTS] = {0};

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
    dw1000_nrng_instance_t * nranges = inst->nrng;

    dw1000_ccp_instance_t * ccp = inst->ccp;
    uint16_t idx = slot->idx;

    hal_gpio_toggle(LED_BLINK_PIN);  
    
#if MYNEWT_VAL(WCS_ENABLED)
    wcs_instance_t * wcs = ccp->wcs;
    uint64_t dx_time = (ccp->epoch + (uint64_t) round((1.0l + wcs->skew) * (double)((idx * (uint64_t)tdma->period << 16)/tdma->nslots)));
#else
    uint64_t dx_time = (ccp->epoch + (uint64_t) (idx * ((uint64_t)tdma->period << 16)/tdma->nslots));
#endif
    dx_time = dx_time & 0xFFFFFFFFFE00UL;
    uint32_t tic = os_cputime_ticks_to_usecs(os_cputime_get32());
    if(dw1000_nrng_request_delay_start(inst, 0xffff, dx_time, DWT_DS_TWR_NRNG, MYNEWT_VAL(NODE_START_SLOT_ID), MYNEWT_VAL(NODE_END_SLOT_ID)).start_tx_error){
        uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
        printf("{\"utime\": %lu,\"msg\": \"slot_timer_cb_%d:start_tx_error\"}\n",utime,idx);
    }
    uint32_t toc = os_cputime_ticks_to_usecs(os_cputime_get32());
    printf("{\"utime\": %lu,\"slot_timer_cb_tic_toc\": %lu}\n",toc,toc-tic);
    
    for(int i=0; i<nranges->nframes/FRAMES_PER_RANGE; i++){
        nrng_frame_t *prev_frame = nranges->frames[i][FIRST_FRAME_IDX];
        nrng_frame_t *frame = nranges->frames[i][SECOND_FRAME_IDX];

        if ((frame->code == DWT_DS_TWR_NRNG_FINAL && prev_frame->code == DWT_DS_TWR_NRNG_T2)\
             || (prev_frame->code == DWT_DS_TWR_NRNG_EXT_T2 && frame->code == DWT_DS_TWR_NRNG_EXT_FINAL)) {
            float range = dw1000_rng_tof_to_meters(dw1000_nrng_twr_to_tof_frames(inst, frame, prev_frame));
            printf("  src_addr= 0x%X  dst_addr= 0x%X  range= %lu\n",prev_frame->src_address,prev_frame->dst_address, (uint32_t)(range*1000));
            frame->code = DWT_DS_TWR_NRNG_END;
            prev_frame->code = DWT_DS_TWR_NRNG_END;
        }else if(prev_frame->code == DWT_SS_TWR_NRNG_FINAL){
            float range = dw1000_rng_tof_to_meters(dw1000_nrng_twr_to_tof_frames(inst, prev_frame, prev_frame));
            printf("  src_addr= 0x%X  dst_addr= 0x%X  range= %lu\n",prev_frame->src_address,prev_frame->dst_address, (uint32_t)(range*1000));
            prev_frame->code = DWT_DS_TWR_NRNG_END;
        }
    }
}
/*! 
 * @fn slot0_timer_cb(struct os_event * ev)
 * @brief This function is a place holder
 *
 * input parameters
 * @param inst - struct os_event *  
 * output parameters
 * returns none 
static void 
slot0_timer_cb(struct os_event *ev){
    //printf("{\"utime\": %lu,\"msg\": \"%s:[%d]:slot0_timer_cb\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()),__FILE__, __LINE__); 
}
 */

#define SLOT MYNEWT_VAL(SLOT_ID)

int main(int argc, char **argv){
    int rc;

    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);

    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);

#if MYNEWT_VAL(CCP_ENABLED)
    dw1000_ccp_start(inst, CCP_ROLE_SLAVE);
#endif
    printf("device_id = 0x%lX\n",inst->device_id);
    printf("PANID = 0x%X\n",inst->PANID);
    printf("DeviceID = 0x%X\n",inst->my_short_address);
    printf("partID = 0x%lX\n",inst->partID);
    printf("lotID = 0x%lX\n",inst->lotID);
    printf("xtal_trim = 0x%X\n",inst->xtal_trim);

   for (uint16_t i = 0; i < sizeof(g_slot)/sizeof(uint16_t); i++)
        g_slot[i] = i;
    tdma_assign_slot(inst->tdma, slot_timer_cb, g_slot[SLOT], &g_slot[SLOT]);
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}
