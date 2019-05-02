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
#include <float.h>
#include "sysinit/sysinit.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_bsp.h"
#include "imgmgr/imgmgr.h"
#include <dw1000/dw1000_dev.h>
#include <dw1000/dw1000_hal.h>
#include <dw1000/dw1000_phy.h>
#include <dw1000/dw1000_mac.h>
#include <dw1000/dw1000_ftypes.h>
#include <tdma/tdma.h>
#if MYNEWT_VAL(CCP_ENABLED)
#include <ccp/ccp.h>
#endif
#if MYNEWT_VAL(NRNG_ENABLED)
#include <nrng/nrng.h>
#endif
#if MYNEWT_VAL(TIMESCALE)
#include <timescale/timescale.h> 
#endif
#if MYNEWT_VAL(WCS_ENABLED)
#include <wcs/wcs.h>
#endif
#if MYNEWT_VAL(SURVEY_ENABLED)
#include <survey/survey.h>
#endif

#if MYNEWT_VAL(PAN_ENABLED)
#include <pan/pan.h>

#if MYNEWT_VAL(PANMASTER_ISSUER)
#include <panmaster/panmaster.h>
#endif

//! Network Roles
typedef enum {
    NTWR_ROLE_INVALID = 0x0,   /*!< Invalid role */
    NTWR_ROLE_NODE,            /*!< NTWR Node */
    NTWR_ROLE_TAG              /*!< NTWR Tag */
} ntwr_role_t;

#endif

static void 
pan_slot_timer_cb(struct os_event * ev)
{
    assert(ev);
    tdma_slot_t * slot = (tdma_slot_t *) ev->ev_arg;
    tdma_instance_t * tdma = slot->parent;
    dw1000_dev_instance_t * inst = tdma->parent;
    uint16_t idx = slot->idx;
    //printf("idx %02d pan slt:%d\n", idx, inst->slot_id);
    
#if MYNEWT_VAL(PANMASTER_ISSUER)
    /* Send pan-reset packages at startup to release all leases */
    static uint8_t _pan_cycles = 0;

    if (_pan_cycles++ < 8) {
        dw1000_pan_reset(inst, tdma_tx_slot_start(inst, idx));
    } else {
        uint64_t dx_time = tdma_rx_slot_start(inst, idx);
        dw1000_set_rx_timeout(inst, 3*inst->ccp->period/tdma->nslots/4);
        dw1000_set_delay_start(inst, dx_time);
        dw1000_set_on_error_continue(inst, true);
        dw1000_pan_listen(inst, DWT_BLOCKING);
    }
#else

    if (inst->pan->status.valid && dw1000_pan_lease_remaining(inst)>MYNEWT_VAL(PAN_LEASE_EXP_MARGIN)) {
        uint16_t timeout;
        if (inst->pan->config->role == PAN_ROLE_RELAY) {
            timeout = 3*inst->ccp->period/tdma->nslots/4;
        } else {
            /* Only listen long enough to get any resets from master */
            timeout = dw1000_phy_frame_duration(&inst->attrib, sizeof(sizeof(struct _pan_frame_t)))
                + MYNEWT_VAL(XTALT_GUARD);
        }
        dw1000_set_rx_timeout(inst, timeout);
        dw1000_set_delay_start(inst, tdma_rx_slot_start(inst, idx));
        dw1000_set_on_error_continue(inst, true);
        if (dw1000_pan_listen(inst, DWT_BLOCKING).start_rx_error) {
            printf("{\"utime\": %lu,\"msg\": \"pan_listen_err\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()));
        }
    } else {
        /* Subslot 0 is for master reset, subslot 1 is for sending requests */
        uint64_t dx_time = tdma_tx_slot_start(inst, idx + 1.0f/16);
        dw1000_pan_blink(inst, NTWR_ROLE_NODE, DWT_BLOCKING, dx_time);
    }
#endif // PANMASTER_ISSUER
}

static void nrng_complete_cb(struct os_event *ev) {
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);

    hal_gpio_toggle(LED_BLINK_PIN);
    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *)ev->ev_arg;
    dw1000_nrng_instance_t * nranges = inst->nrng;
    nrng_frame_t * frame = nranges->frames[(nranges->idx)%nranges->nframes];

#ifdef VERBOSE
    if (inst->status.start_rx_error)
        printf("{\"utime\": %lu,\"timer_ev_cb\": \"start_rx_error\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()));
    if (inst->status.start_tx_error)
        printf("{\"utime\": %lu,\"timer_ev_cb\":\"start_tx_error\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()));
    if (inst->status.rx_error)
        printf("{\"utime\": %lu,\"timer_ev_cb\":\"rx_error\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()));
    if (inst->status.rx_timeout_error)
        printf("{\"utime\": %lu,\"timer_ev_cb\":\"rx_timeout_error\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()));
#endif
    if (frame->code == DWT_DS_TWR_NRNG_FINAL || frame->code == DWT_DS_TWR_NRNG_EXT_FINAL){
        frame->code = DWT_DS_TWR_NRNG_END;
    }
}
/*! 
 * @fn complete_cb(dw1000_dev_instance_t * inst)
 *
 * @brief This callback is in the interrupt context and is uses to schedule an pdoa_complete event on the default event queue.  
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
static struct os_callout slot_callout;
static bool complete_cb(dw1000_dev_instance_t * inst, dw1000_mac_interface_t * cbs){
    if(inst->fctrl != FCNTL_IEEE_RANGE_16){
        return false;
    }
    os_callout_init(&slot_callout, os_eventq_dflt_get(), nrng_complete_cb, inst);
    os_eventq_put(os_eventq_dflt_get(), &slot_callout.c_ev);
    return true;
}

/*! 
 * @fn slot_timer_cb(struct os_event * ev)
 *
 * @brief In this example this timer callback is used to start_rx.
 *
 * input parameters
 * @param inst - struct os_event *  
 *
 * output parameters
 *
 * returns none 
 */

    
static void 
slot_cb(struct os_event * ev){
    assert(ev);

    tdma_slot_t * slot = (tdma_slot_t *) ev->ev_arg;
    tdma_instance_t * tdma = slot->parent;
    dw1000_dev_instance_t * inst = tdma->parent;
    uint16_t idx = slot->idx;

    dw1000_set_delay_start(inst, tdma_rx_slot_start(inst, idx));
    uint16_t timeout = dw1000_phy_frame_duration(&inst->attrib, sizeof(nrng_request_frame_t))
                        + inst->nrng->config.rx_timeout_delay;    
          
    dw1000_set_rx_timeout(inst, timeout + 0x1000);
    dw1000_nrng_listen(inst, DWT_BLOCKING);
}

#if MYNEWT_VAL(PANMASTER_ISSUER) == 0
static void
pan_complete_cb(struct os_event * ev)
{
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);
    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *)ev->ev_arg;
    
    if (inst->slot_id != 0xffff) {
        uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
        printf("{\"utime\": %lu,\"msg\": \"slot_id = %d\"}\n", utime, inst->slot_id);
        printf("{\"utime\": %lu,\"msg\": \"euid16 = 0x%X\"}\n", utime, inst->my_short_address);
    }
}
#endif

#if MYNEWT_VAL(PANMASTER_ISSUER) == 0
/* This function allows the ccp to compensate for the time of flight
 * from the master anchor to the current anchor. 
 * Ideally this should use a map generated and make use of the euid in case 
 * the ccp packet is relayed through another node.
 */
static uint32_t 
tof_comp_cb(uint64_t euid, uint16_t short_addr) 
{
    float x = MYNEWT_VAL(CCP_TOF_COMP_LOCATION_X);
    float y = MYNEWT_VAL(CCP_TOF_COMP_LOCATION_Y);
    float z = MYNEWT_VAL(CCP_TOF_COMP_LOCATION_Z);
    float dist_in_meters = sqrtf(x*x+y*y+z*z);
#ifdef VERBOSE
    printf("d=%dm, %ld dwunits\n", (int)dist_in_meters,
           (uint32_t)(dist_in_meters/dw1000_rng_tof_to_meters(1.0)));
#endif
    return dist_in_meters/dw1000_rng_tof_to_meters(1.0);
}
#endif

int main(int argc, char **argv){
    int rc;

    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);

    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
    inst->config.rxauto_enable = false;
    inst->config.dblbuffon_enabled = true;
    dw1000_set_dblrxbuff(inst, inst->config.dblbuffon_enabled);  

    dw1000_mac_interface_t cbs = (dw1000_mac_interface_t){
        .id =  DW1000_APP0,
        .complete_cb = complete_cb
    };

    dw1000_mac_append_interface(inst, &cbs);
    inst->slot_id = 0xffff;
    inst->my_long_address = ((uint64_t) inst->lotID << 32) + inst->partID;

#if MYNEWT_VAL(PANMASTER_ISSUER)
    /* Pan-master also is the clock-master */
    dw1000_ccp_start(inst, CCP_ROLE_MASTER);

    /* As pan-master, first lookup our address and slot_id */
    struct image_version fw_ver;
    struct panmaster_node *node;
    panmaster_find_node(inst->my_long_address, NTWR_ROLE_NODE, &node);
    assert(node);
    /* Update my fw-version in the panmaster db */
    imgr_my_version(&fw_ver);
    panmaster_add_version(inst->my_long_address, &fw_ver);
    /* Set short address and slot id */
    inst->my_short_address = node->addr;
    inst->slot_id = node->slot_id;
    dw1000_pan_start(inst, PAN_ROLE_MASTER);
#else    
    dw1000_ccp_start(inst, CCP_ROLE_SLAVE);
    dw1000_ccp_set_tof_comp_cb(inst->ccp, tof_comp_cb);

    dw1000_pan_set_postprocess(inst, pan_complete_cb);
    dw1000_pan_start(inst, PAN_ROLE_RELAY);
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

    /* Pan is slots 1&2 */
    tdma_assign_slot(inst->tdma, pan_slot_timer_cb, 1, NULL);
    tdma_assign_slot(inst->tdma, pan_slot_timer_cb, 2, NULL);
    
#if MYNEWT_VAL(SURVEY_ENABLED)
    tdma_assign_slot(inst->tdma, survey_slot_range_cb, MYNEWT_VAL(SURVEY_RANGE_SLOT), NULL);
    tdma_assign_slot(inst->tdma, survey_slot_broadcast_cb, MYNEWT_VAL(SURVEY_BROADCAST_SLOT), NULL);
    for (uint16_t i = 6; i < MYNEWT_VAL(TDMA_NSLOTS); i++)
#else
    for (uint16_t i = 3; i < MYNEWT_VAL(TDMA_NSLOTS); i++)
#endif
        tdma_assign_slot(inst->tdma, slot_cb, i, NULL);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    assert(0);
    return rc;
}

