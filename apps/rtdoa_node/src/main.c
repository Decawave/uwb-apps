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
#include "imgmgr/imgmgr.h"
#include "hal/hal_gpio.h"
#include "hal/hal_bsp.h"
#include <hal/hal_system.h>
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif

#if MYNEWT_VAL(BLEPRPH_ENABLED)
#include "bleprph/bleprph.h"
#endif

#include "dw1000/dw1000_dev.h"
#include "dw1000/dw1000_hal.h"
#include "dw1000/dw1000_phy.h"
#include "dw1000/dw1000_mac.h"
#include "dw1000/dw1000_ftypes.h"
#include "uwbcfg/uwbcfg.h"
#include <config/config.h>

#if MYNEWT_VAL(NMGR_UWB_ENABLED)
#include <nmgr_uwb/nmgr_uwb.h> 
#endif

#include <tdma/tdma.h>
#include <ccp/ccp.h>
#include <wcs/wcs.h>
#include <timescale/timescale.h> 
#if MYNEWT_VAL(RNG_ENABLED)
#include <rng/rng.h>
#endif
#if MYNEWT_VAL(NRNG_ENABLED)
#include <nrng/nrng.h>
#endif
#include <rtdoa/rtdoa.h>
#include <rtdoa_node/rtdoa_node.h>
#include <tofdb/tofdb.h>

//#define DIAGMSG(s,u) printf(s,u)
#ifndef DIAGMSG
#define DIAGMSG(s,u)
#endif


#if MYNEWT_VAL(PAN_ENABLED)
#include <pan/pan.h>

#include "panmaster/panmaster.h"

//! Network Roles
typedef enum {
    NTWR_ROLE_INVALID = 0x0,   /*!< Invalid role */
    NTWR_ROLE_NODE,            /*!< NTWR Node */
    NTWR_ROLE_TAG              /*!< NTWR Tag */
} ntwr_role_t;

#endif

static bool dw1000_config_updated = false;
int
uwb_config_updated()
{
    /* Workaround in case we're stuck waiting for ccp with the 
     * wrong radio settings */
    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
    dw1000_ccp_instance_t *ccp = (dw1000_ccp_instance_t*)dw1000_mac_find_cb_inst_ptr(inst, DW1000_CCP);
    if (os_sem_get_count(&ccp->sem) == 0 || !ccp->status.valid) {
        dw1000_mac_config(inst, NULL);
        dw1000_phy_config_txrf(inst, &inst->config.txrf);
        if (os_sem_get_count(&ccp->sem) == 0) {
            dw1000_start_rx(inst);
        }
        return 0;
    }

    dw1000_config_updated = true;
    return 0;
}
struct uwbcfg_cbs uwb_cb = {
    .uc_update = uwb_config_updated
};

static void 
pan_slot_timer_cb(struct os_event * ev)
{
    assert(ev);
    tdma_slot_t * slot = (tdma_slot_t *) ev->ev_arg;
    tdma_instance_t * tdma = slot->parent;
    dw1000_ccp_instance_t *ccp = tdma->ccp;
    dw1000_dev_instance_t * inst = tdma->dev_inst;
    dw1000_pan_instance_t *pan = (dw1000_pan_instance_t*)slot->arg;

    uint16_t idx = slot->idx;
    hal_gpio_write(LED_BLINK_PIN, idx%2);
    // printf("idx %02d pan slt:%d\n", idx, inst->slot_id);

    if (dw1000_config_updated) {
        dw1000_mac_config(inst, NULL);
        dw1000_phy_config_txrf(inst, &inst->config.txrf);
        dw1000_config_updated = false;
    }
    
    if (inst->role&DW1000_ROLE_PAN_MASTER) {
        /* Act as a Master Node in the network */
        static int _pan_cycles = 0;

        /* Broadcast a reset message to clear all leases */
        if (_pan_cycles++ < 8) {
            dw1000_pan_reset(pan, tdma_tx_slot_start(tdma, idx));
        } else {
            uint64_t dx_time = tdma_rx_slot_start(tdma, idx);
            dw1000_set_rx_timeout(inst, 3*ccp->period/tdma->nslots/4);
            dw1000_set_delay_start(inst, dx_time);
            dw1000_set_on_error_continue(inst, true);
            dw1000_pan_listen(pan, DWT_BLOCKING);
        }
    } else {
        /* Act as a slave Node in the network */
        if (pan->status.valid && dw1000_pan_lease_remaining(pan)>MYNEWT_VAL(PAN_LEASE_EXP_MARGIN)) {
            uint16_t timeout;
            if (pan->config->role == PAN_ROLE_RELAY) {
                timeout = 3*ccp->period/tdma->nslots/4;
            } else {
                /* Only listen long enough to get any resets from master */
                timeout = dw1000_phy_frame_duration(&inst->attrib, sizeof(sizeof(struct _pan_frame_t)))
                    + MYNEWT_VAL(XTALT_GUARD);
            }
            dw1000_set_rx_timeout(inst, timeout);
            dw1000_set_delay_start(inst, tdma_rx_slot_start(tdma, idx));
            dw1000_set_on_error_continue(inst, true);
            if (dw1000_pan_listen(pan, DWT_BLOCKING).start_rx_error) {
                DIAGMSG("{\"utime\": %lu,\"msg\": \"pan_listen_err\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()));
            }
        } else {
            /* Subslot 0 is for master reset, subslot 1 is for sending requests */
            uint64_t dx_time = tdma_tx_slot_start(tdma, (float)idx+1.0f/16);
            dw1000_pan_blink(pan, NTWR_ROLE_NODE, DWT_BLOCKING, dx_time);
        }
    }
}
    


/**
 * @fn nrng_slot_timer_cb(struct os_event * ev)
 *
 * @brief Node to Node ranging for tof timing compensation
 *
 */
static void 
nrng_slot_timer_cb(struct os_event *ev)
{
    assert(ev);
    tdma_slot_t * slot = (tdma_slot_t *) ev->ev_arg;
    tdma_instance_t * tdma = slot->parent;
    dw1000_nrng_instance_t * nrng = (dw1000_nrng_instance_t *)slot->arg;
    dw1000_dev_instance_t * inst = tdma->dev_inst;
    dw1000_ccp_instance_t * ccp = tdma->ccp;

    uint16_t idx = slot->idx;

    /* Avoid colliding with the ccp */
    if (os_sem_get_count(&ccp->sem) == 0 || idx == 0xffff) {
        return;
    }

    uint16_t anchor_rng_initiator = ccp->seq_num % 8;
    if (anchor_rng_initiator == inst->slot_id) {
        uint64_t dx_time = tdma_tx_slot_start(tdma, idx) & 0xFFFFFFFFFE00UL;
        uint32_t slot_mask = 0xFFFF;

        if(dw1000_nrng_request_delay_start(nrng, BROADCAST_ADDRESS, dx_time,
                                           DWT_SS_TWR_NRNG, slot_mask, 0).start_tx_error){
            /* Do nothing */
        }
    } else {
        dw1000_set_delay_start(inst, tdma_rx_slot_start(tdma, idx));
        uint16_t timeout = dw1000_phy_frame_duration(&inst->attrib, sizeof(nrng_request_frame_t))
            + nrng->config.rx_timeout_delay;

        dw1000_set_rx_timeout(inst, timeout + 0x100);
        dw1000_nrng_listen(nrng, DWT_BLOCKING);
    }
}

static void
nrng_complete_cb(struct os_event *ev) {
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);

    dw1000_nrng_instance_t * nrng = (dw1000_nrng_instance_t *)ev->ev_arg;
    nrng_frame_t * frame = nrng->frames[(nrng->idx)%nrng->nframes];

    for (int i=0;i<nrng->nframes;i++) {
        frame = nrng->frames[(nrng->idx + i)%nrng->nframes];
        uint16_t dst_addr = frame->dst_address;
        if (frame->code != DWT_SS_TWR_NRNG_FINAL || frame->seq_num != nrng->seq_num) {
            continue;
        }

        float tof = dw1000_nrng_twr_to_tof_frames(nrng->dev_inst, frame, frame);
        // float rssi = dw1000_calc_rssi(inst, &frame->diag);
        tofdb_set_tof(dst_addr, tof);
    }
}

static struct os_event slot_event;
static bool complete_cb(dw1000_dev_instance_t * inst, dw1000_mac_interface_t * cbs)
{
    if(inst->fctrl != FCNTL_IEEE_RANGE_16){
        return false;
    }
    os_eventq_put(os_eventq_dflt_get(), &slot_event);
    return true;
}

/**
 * @fn rtdoa_slot_timer_cb(struct os_event * ev)
 *
 * @brief RTDoA Emission slot
 *
 */
static void 
rtdoa_slot_timer_cb(struct os_event *ev)
{
    assert(ev);
    tdma_slot_t * slot = (tdma_slot_t *) ev->ev_arg;
    uint16_t idx = slot->idx;
    // printf("rtdoa %02d\n", idx);
    tdma_instance_t * tdma = slot->parent;
    assert(tdma);
    dw1000_ccp_instance_t * ccp = tdma->ccp;
    assert(ccp);
    dw1000_dev_instance_t * inst = tdma->dev_inst;
    assert(inst);
    dw1000_rtdoa_instance_t* rtdoa = (dw1000_rtdoa_instance_t*)slot->arg;
    assert(rtdoa);

    /* Avoid colliding with the ccp */
    if (os_sem_get_count(&ccp->sem) == 0) {
        return;
    }

    if (inst->role&DW1000_ROLE_CCP_MASTER) {
        uint64_t dx_time = tdma_tx_slot_start(tdma, idx) & 0xFFFFFFFFFE00UL;

        if(dw1000_rtdoa_request(rtdoa, dx_time).start_tx_error) {
            /* Do nothing */
            printf("rtdoa_start_err\n");
        }
    } else {
        uint64_t dx_time = tdma_rx_slot_start(tdma, idx);
        if(dw1000_rtdoa_listen(rtdoa, DWT_BLOCKING, dx_time, 3*ccp->period/tdma->nslots/4).start_rx_error) {
            printf("#rse\n");
        }
    }
}

static void 
nmgr_slot_timer_cb(struct os_event * ev)
{
    assert(ev);
    tdma_slot_t * slot = (tdma_slot_t *) ev->ev_arg;
    tdma_instance_t * tdma = slot->parent;
    dw1000_ccp_instance_t * ccp = tdma->ccp;
    uint16_t idx = slot->idx;
    nmgr_uwb_instance_t* nmgruwb = (nmgr_uwb_instance_t*)slot->arg;
    assert(nmgruwb);
    // printf("idx %02d nmgr\n", idx);

    /* Avoid colliding with the ccp */
    if (os_sem_get_count(&ccp->sem) == 0) {
        return;
    }

    uint16_t timeout = 3*ccp->period/tdma->nslots/4;
    if (uwb_nmgr_process_tx_queue(nmgruwb, tdma_tx_slot_start(tdma, idx)) == false) {
        nmgr_uwb_listen(nmgruwb, DWT_BLOCKING, tdma_rx_slot_start(tdma, idx), timeout);
    }
}


static void
tdma_allocate_slots(dw1000_dev_instance_t * inst)
{
    uint16_t i;
    /* Pan is slot 1,2 */
    tdma_instance_t * tdma = (tdma_instance_t*)dw1000_mac_find_cb_inst_ptr(inst, DW1000_TDMA);
    assert(tdma);
    dw1000_pan_instance_t *pan = (dw1000_pan_instance_t*)dw1000_mac_find_cb_inst_ptr(inst, DW1000_PAN);
    assert(pan);
    tdma_assign_slot(tdma, pan_slot_timer_cb, 1, (void*)pan);
    tdma_assign_slot(tdma, pan_slot_timer_cb, 2, (void*)pan);

    /* anchor-to-anchor range slot is 31 */
    dw1000_nrng_instance_t * nrng = (dw1000_nrng_instance_t *)dw1000_mac_find_cb_inst_ptr(inst, DW1000_NRNG);
    assert(nrng);
    tdma_assign_slot(tdma, nrng_slot_timer_cb, 31, (void*)nrng);

    nmgr_uwb_instance_t *nmgruwb = (nmgr_uwb_instance_t*)dw1000_mac_find_cb_inst_ptr(inst, DW1000_NMGR_UWB);
    assert(nmgruwb);
    dw1000_rtdoa_instance_t* rtdoa = (dw1000_rtdoa_instance_t*)dw1000_mac_find_cb_inst_ptr(inst, DW1000_RTDOA);
    assert(rtdoa);
    
    for (i=3;i < MYNEWT_VAL(TDMA_NSLOTS);i++) {
        if (i==31) {
            continue;
        }
        if (i%12==0) {
            tdma_assign_slot(tdma, nmgr_slot_timer_cb, i, (void*)nmgruwb);
            /* TODO REMOVE BELOW */
        } else if (i%2==0){
            tdma_assign_slot(tdma, rtdoa_slot_timer_cb, i, (void*)rtdoa);
        }
    }
}

int
main(int argc, char **argv)
{
    int rc = 0;

    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);

    uwbcfg_register(&uwb_cb);
    conf_load();

    static dw1000_mac_interface_t cbs = (dw1000_mac_interface_t){
        .id =  DW1000_APP0,
        .complete_cb = complete_cb
    };
    
    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
    dw1000_mac_append_interface(inst, &cbs);
    slot_event.ev_cb  = nrng_complete_cb;
    dw1000_nrng_instance_t * nrng = (dw1000_nrng_instance_t *)dw1000_mac_find_cb_inst_ptr(inst, DW1000_NRNG);
    assert(nrng);
    slot_event.ev_arg = (void*)nrng;

    inst->config.rxauto_enable = 0;
    inst->config.trxoff_enable = 1;
    inst->config.rxdiag_enable = 1;
    inst->config.sleep_enable = 1;
    inst->config.dblbuffon_enabled = 0;
    dw1000_set_dblrxbuff(inst, false);

    inst->slot_id = 0xffff;
    inst->my_short_address = inst->partID&0xffff;
    inst->my_long_address = ((uint64_t) inst->lotID << 32) + inst->partID;
    dw1000_set_address16(inst,inst->my_short_address);

    ble_init(inst->my_long_address);

    dw1000_ccp_instance_t *ccp = (dw1000_ccp_instance_t*)dw1000_mac_find_cb_inst_ptr(inst, DW1000_CCP);
    assert(ccp);
    dw1000_pan_instance_t *pan = (dw1000_pan_instance_t*)dw1000_mac_find_cb_inst_ptr(inst, DW1000_PAN);
    assert(pan);

    if (inst->role&DW1000_ROLE_CCP_MASTER) {
        printf("{\"role\"=\"ccp_master\"}\n");
        dw1000_ccp_start(ccp, CCP_ROLE_MASTER);

        struct image_version fw_ver;
        struct panmaster_node *node;
        panmaster_find_node(inst->my_long_address, NTWR_ROLE_NODE, &node);
        assert(node);
        imgr_my_version(&fw_ver);
        panmaster_add_version(inst->my_long_address, &fw_ver);
        inst->my_short_address = node->addr;
        inst->slot_id = node->slot_id;
        dw1000_pan_start(pan, PAN_ROLE_MASTER);
    } else {
        dw1000_ccp_start(ccp, CCP_ROLE_RELAY);
        dw1000_pan_start(pan, PAN_ROLE_RELAY);
    }
    printf("{\"device_id\"=\"%lX\"",inst->device_id);
    printf(",\"PANID=\"%X\"",inst->PANID);
    printf(",\"addr\"=\"%X\"",inst->my_short_address);
    printf(",\"partID\"=\"%lX\"",inst->partID);
    printf(",\"lotID\"=\"%lX\"",inst->lotID);
    printf(",\"slot_id\"=\"%d\"",inst->slot_id);
    printf(",\"xtal_trim\"=\"%X\"}\n",inst->xtal_trim);

    tdma_allocate_slots(inst);

#if MYNEWT_VAL(NCBWIFI_ESP_PASSTHROUGH)
    hal_bsp_esp_bypass(true);
#endif

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}

