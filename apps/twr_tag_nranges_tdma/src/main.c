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

#include <pan/pan.h>

//! Network Roles
typedef enum {
    NTWR_ROLE_INVALID = 0x0,   /*!< Invalid role */
    NTWR_ROLE_NODE,            /*!< NTWR Node */
    NTWR_ROLE_TAG              /*!< NTWR Tag */
} ntwr_role_t;
static void pan_slot_timer_cb(struct os_event * ev);

//#define DIAGMSG(s,u) printf(s,u)
#ifndef DIAGMSG
#define DIAGMSG(s,u)
#endif


static bool dw1000_config_updated = false;


/*! 
 * @fn slot_cb(struct os_event * ev)
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
    dw1000_ccp_instance_t * ccp = inst->ccp;

    if (ccp->local_epoch==0 || inst->pan->status.valid == false) return;
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

/*! 
 * @fn tdma_allocate_slots(struct os_event * ev)
 *
 * @brief This function assigns the ranging slots for this tag
 * according to the tag-slot_id we've received.
 */
static void
tdma_allocate_slots(dw1000_dev_instance_t * inst, bool reset_tdma)
{
    static int old_slot_id = -1;
#ifdef PAN_VERBOSE
    printf("tdma_allocate_slots for slot_id:%d\n", inst->slot_id);
#endif

    if (old_slot_id == inst->slot_id) {
        /* Using the same slots as previously, no need to do anything */
        return;
    }
    old_slot_id = inst->slot_id;

    /* Slots 0-4 are management slots (ccp, pan, anchor ranging) */
    for (uint16_t i = 5;
         (i+MYNEWT_VAL(NRNG_NTAGS)) < MYNEWT_VAL(TDMA_NSLOTS);
         i+=(MYNEWT_VAL(NRNG_NTAGS))) {
        
        for (int j=0;j<MYNEWT_VAL(NRNG_NTAGS);j++) {
            tdma_release_slot(inst->tdma, i+j);
            if (j == inst->slot_id) {
                tdma_assign_slot(inst->tdma, slot_cb, i+j, NULL);
#ifdef PAN_VERBOSE
                printf("tdma_slot:%03d\n", i);
#endif
            }
        }
    }
    
    if (reset_tdma) {
#ifdef TDMA_TASKS_ENABLE
        os_eventq_put(&inst->tdma->eventq, &inst->tdma->event_cb.c_ev);
#else
        os_eventq_put(&inst->eventq, &inst->tdma->event_cb.c_ev);
#endif
    }
}

static void
pan_complete_cb(struct os_event * ev)
{
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);
    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *)ev->ev_arg;
    
    if (inst->slot_id != 0xffff) {
        tdma_allocate_slots(inst, true);
    }
}

static void 
pan_slot_timer_cb(struct os_event * ev)
{
    assert(ev);

    tdma_slot_t * slot = (tdma_slot_t *) ev->ev_arg;
    tdma_instance_t * tdma = slot->parent;
    dw1000_dev_instance_t * inst = tdma->parent;
    uint16_t idx = slot->idx;

    if (dw1000_config_updated) {
        dw1000_mac_config(inst, NULL);
        dw1000_phy_config_txrf(inst, &inst->config.txrf);
        dw1000_config_updated = false;
        dw1000_set_dblrxbuff(inst, true);
    }
    hal_gpio_write(LED_BLINK_PIN, 1);

    /* Check if out lease has run out and if so renew, otherwise just listen
     * for possible network resets / changes */
    if (inst->pan->status.valid &&
        dw1000_pan_lease_remaining(inst)>MYNEWT_VAL(PAN_LEASE_EXP_MARGIN)) {
        uint16_t timeout = dw1000_phy_frame_duration(&inst->attrib, sizeof(sizeof(struct _pan_frame_t)))
            + MYNEWT_VAL(XTALT_GUARD);
        dw1000_set_rx_timeout(inst, timeout);
        dw1000_set_delay_start(inst, tdma_rx_slot_start(inst, idx));
        dw1000_pan_listen(inst, DWT_BLOCKING);
    } else {
        /* We need to renew / aquire a lease */
        /* Subslot 0 is for master reset, subslot 1 is for sending requests */
        uint64_t dx_time = tdma_tx_slot_start(inst, idx + 1.0f/16);
        dw1000_pan_blink(inst, NTWR_ROLE_TAG, DWT_BLOCKING, dx_time);
    }
    hal_gpio_write(LED_BLINK_PIN, 0);
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
    inst->slot_id = 0xffff;
    inst->my_long_address = ((uint64_t) inst->lotID << 32) + inst->partID;
    
#if MYNEWT_VAL(CCP_ENABLED)
    dw1000_ccp_start(inst, CCP_ROLE_SLAVE);
#endif

    dw1000_pan_set_postprocess(inst, pan_complete_cb);
    dw1000_pan_start(inst, PAN_ROLE_SLAVE);

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

    /* Pan is assigned to slots 1 and 2 */
    tdma_assign_slot(inst->tdma, pan_slot_timer_cb, 1, NULL);
    tdma_assign_slot(inst->tdma, pan_slot_timer_cb, 2, NULL);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}
