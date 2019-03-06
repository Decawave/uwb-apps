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
#include <nrng/nrng.h>
#endif
#if MYNEWT_VAL(SURVEY_ENABLED)
#include <survey/survey.h>
#endif
#if MYNEWT_VAL(UWBCFG_ENABLED)
#include <config/config.h>
#include "uwbcfg/uwbcfg.h"
#endif

//#define DIAGMSG(s,u) printf(s,u)
#ifndef DIAGMSG
#define DIAGMSG(s,u)
#endif
#ifndef TICTOC
#undef TICTOC
#endif


static bool dw1000_config_updated = false;

void print_frame(nrng_frame_t * twr, char crlf){
    printf("{\"utime\": %lu,%c\"fctrl\": \"0x%04X\",%c", os_cputime_ticks_to_usecs(os_cputime_get32()), crlf, twr->fctrl, crlf);
    printf("\"seq_num\": \"0x%02X\",%c", twr->seq_num, crlf);
    printf("\"PANID\": \"0x%04X\",%c", twr->PANID, crlf);
    printf("\"dst_address\": \"0x%04X\",%c", twr->dst_address, crlf);
    printf("\"src_address\": \"0x%04X\",%c", twr->src_address, crlf);
    printf("\"code\": \"0x%04X\",%c", twr->code, crlf);
    printf("\"reception_timestamp\": \"0x%08lX\",%c", twr->reception_timestamp, crlf);
    printf("\"transmission_timestamp\": \"0x%08lX\",%c", twr->transmission_timestamp, crlf);
    printf("\"request_timestamp\": \"0x%08lX\",%c", twr->request_timestamp, crlf);
    printf("\"response_timestamp\": \"0x%08lX\"}\n", twr->response_timestamp);
}

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
slot_cb(struct os_event *ev){
    assert(ev);
    assert(ev->ev_arg);

    tdma_slot_t * slot = (tdma_slot_t *) ev->ev_arg;
    tdma_instance_t * tdma = slot->parent;
    dw1000_dev_instance_t * inst = tdma->parent;
    uint16_t idx = slot->idx;

    hal_gpio_toggle(LED_BLINK_PIN);  

    uint64_t dx_time = tdma_tx_slot_start(inst, idx) & 0xFFFFFFFFFE00UL;
    uint32_t slot_mask = 0;
    for (uint16_t i = MYNEWT_VAL(NODE_START_SLOT_ID); i <= MYNEWT_VAL(NODE_END_SLOT_ID); i++)
        slot_mask |= 1UL << i;
        
    if(dw1000_nrng_request_delay_start(inst, BROADCAST_ADDRESS, dx_time, DWT_SS_TWR_NRNG, slot_mask, 0).start_tx_error){
        uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
        printf("{\"utime\": %lu,\"msg\": \"slot_timer_cb_%d:start_tx_error\"}\n",utime,idx);
    }else{
        #if !MYNEWT_VAL(NRNG_VERBOSE)
            // Place holder 
        #endif
    }
}
/**
 * @fn uwb_config_update
 * 
 * Called from the main event queue as a result of the uwbcfg packet
 * having received a commit/load of new uwb configuration.
 */
int
uwb_config_updated()
{
    dw1000_config_updated = true;
    return 0;
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

int main(int argc, char **argv){
    int rc;

    sysinit();
    
    /* Register callback for UWB configuration changes */
    struct uwbcfg_cbs uwb_cb = {
        .uc_update = uwb_config_updated
    };
    uwbcfg_register(&uwb_cb);
    /* Load config from flash */
    conf_load();

    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);

    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
    inst->config.cir_enable = false;
    inst->config.rxauto_enable = false;
    inst->config.dblbuffon_enabled = true;
    dw1000_set_dblrxbuff(inst, inst->config.dblbuffon_enabled);  
    
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

    for (uint16_t i = 4; i < sizeof(g_slot)/sizeof(uint16_t); i+=2){
       g_slot[i] = i;
       tdma_assign_slot(inst->tdma, slot_cb, g_slot[i], &g_slot[i]);
    }
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}
