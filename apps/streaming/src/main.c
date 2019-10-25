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
#include "imgmgr/imgmgr.h"
#include "hal/hal_gpio.h"
#include "hal/hal_bsp.h"
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif
#if MYNEWT_VAL(BLE_ENABLED)
#include "bleprph/bleprph.h"
#endif

#include <uwb/uwb.h>
#include <uwb/uwb_ftypes.h>
#include <uwb_rng/uwb_rng.h>
#include <config/config.h>
#if MYNEWT_VAL(UWBCFG_ENABLED)
#include "uwbcfg/uwbcfg.h"
#endif
#include <uwb_transport/uwb_transport.h>
#if MYNEWT_VAL(TDMA_ENABLED)
#include <tdma/tdma.h>
#endif
#if MYNEWT_VAL(UWB_CCP_ENABLED)
#include <uwb_ccp/uwb_ccp.h>
#endif
#include <dw1000/dw1000_gpio.h>
#include <dw1000/dw1000_hal.h>
#if MYNEWT_VAL(CONCURRENT_NRNG)
#include <nrng/nrng.h>
#endif

//#define DIAGMSG(s,u) printf(s,u)
#ifndef DIAGMSG
#define DIAGMSG(s,u)
#endif

#if MYNEWT_VAL(UWBCFG_ENABLED)
static bool uwb_config_updated = false;

/**
 * @fn uwb_config_update
 * 
 * Called from the main event queue as a result of the uwbcfg packet
 * having received a commit/load of new uwb configuration.
 */
int
uwb_config_updated_func()
{
    /* Workaround in case we're stuck waiting for ccp with the 
     * wrong radio settings */
    struct uwb_dev * udev = uwb_dev_idx_lookup(0);
    struct uwb_ccp_instance *ccp = (struct uwb_ccp_instance*)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_CCP);
    if (dpl_sem_get_count(&ccp->sem) == 0) {
        uwb_phy_forcetrxoff(udev);
        uwb_mac_config(udev, NULL);
        uwb_txrf_config(udev, &udev->config.txrf);
        uwb_start_rx(udev);
        return 0;
    }

    uwb_config_updated = true;
    return 0;
}
#endif

#if MYNEWT_VAL(CONCURRENT_NRNG)

static void 
range_slot_cb(struct dpl_event * ev){
    assert(ev);

    tdma_slot_t * slot = (tdma_slot_t *) dpl_event_get_arg(ev);
    tdma_instance_t * tdma = slot->parent;
    struct uwb_ccp_instance *ccp = tdma->ccp;
    struct uwb_dev * udev = tdma->dev_inst;
    uint16_t idx = slot->idx;
    struct nrng_instance *nrng = (struct nrng_instance *)slot->arg;

    /* Avoid colliding with the ccp in case we've got out of sync */
    if (dpl_sem_get_count(&ccp->sem) == 0) {
        return;
    }

    if (ccp->local_epoch==0 || udev->slot_id == 0xffff) return;

    if (udev->role&UWB_ROLE_ANCHOR) {
        /* Listen for a ranging tag */
        uwb_set_delay_start(udev, tdma_rx_slot_start(tdma, idx));
        uint16_t timeout = uwb_phy_frame_duration(udev, sizeof(nrng_request_frame_t))
            + nrng->config.rx_timeout_delay;

        /* Padded timeout to allow us to receive any nmgr packets too */
        uwb_set_rx_timeout(udev, timeout + 0x1000);
        nrng_listen(nrng, UWB_BLOCKING);
    } else {
        /* Range with the anchors */
        if (idx%MYNEWT_VAL(NRNG_NTAGS) != udev->slot_id) {
            return;
        }

        /* Range with the anchors */
        uint64_t dx_time = tdma_tx_slot_start(tdma, idx) & 0xFFFFFFFFFE00UL;
        uint32_t slot_mask = 0;
        for (uint16_t i = MYNEWT_VAL(NODE_START_SLOT_ID);
             i <= MYNEWT_VAL(NODE_END_SLOT_ID); i++) {
            slot_mask |= 1UL << i;
        }

        if(nrng_request_delay_start(
               nrng, UWB_BROADCAST_ADDRESS, dx_time,
               DWT_SS_TWR_NRNG, slot_mask, 0).start_tx_error) {
            uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
            printf("{\"utime\": %lu,\"msg\": \"slot_timer_cb_%d:start_tx_error\"}\n",
                   utime,idx);
        }
    }
}
#endif

/*! 
 * @fn slot_cb(struct dpl_event * ev)
 * 
 * In this example, slot_cb schedules the turning of the transceiver to receive mode at the desired epoch. 
 * The precise timing is under the control of the DW1000, and the dw_time variable defines this epoch. 
 * Note the slot_cb itself is scheduled MYNEWT_VAL(OS_LATENCY) in advance of the epoch to set up the transceiver accordingly.
 * 
 * Note: The epoch time is referenced to the Rmarker symbol, it is therefore necessary to advance the rxtime by the SHR_duration such 
 * that the preamble are received. The transceiver, when transmitting adjust the txtime accordingly.
 *
 * input parameters
 * @param inst - struct dpl_event *
 *
 * output parameters
 *
 * returns none 
 */
   
uint8_t test[512 - sizeof(uwb_transport_frame_header_t) - 2]; 
static void 
stream_slot_cb(struct dpl_event * ev)
{
    assert(ev);
    tdma_slot_t * slot = (tdma_slot_t *) dpl_event_get_arg(ev);
    tdma_instance_t * tdma = slot->parent;
    struct uwb_ccp_instance *ccp = tdma->ccp;
   
    uint16_t idx = slot->idx;
    uwb_transport_instance_t * uwb_transport = (uwb_transport_instance_t *)slot->arg;
    /* Avoid colliding with the ccp in case we've got out of sync */
    if (dpl_sem_get_count(&ccp->sem) == 0) {
        return;
    }
#if MYNEWT_VAL(UWBCFG_ENABLED)
    struct uwb_dev * inst = tdma->dev_inst;
    if (uwb_config_updated) {
        uwb_mac_config(inst, NULL);
        uwb_txrf_config(inst, &inst->config.txrf);
        uwb_config_updated = false;
        return;
    }
#endif
    if(uwb_transport_dequeue_tx(uwb_transport, 
            tdma_tx_slot_start(tdma, idx), (tdma_tx_slot_start(tdma, idx+1) >> 16) - (MYNEWT_VAL(OS_LATENCY))) == true){
    }else{
        uwb_transport_listen(uwb_transport, UWB_BLOCKING, 
            tdma_rx_slot_start(tdma, idx), (tdma_rx_slot_start(tdma, idx+1) >> 16) - MYNEWT_VAL(OS_LATENCY));
    }
}

static bool 
uwb_transport_cb(struct uwb_dev * inst, uint16_t uid, struct dpl_mbuf * mbuf){
    dpl_mbuf_free_chain(mbuf);
    return true;
}

#if MYNEWT_VAL(UWB_TRANSPORT_ROLE) == 1 
static struct dpl_callout stream_callout;
static void
stream_timer(struct dpl_event *ev) 
{
    dpl_callout_reset(&stream_callout, OS_TICKS_PER_SEC/60);

    uwb_transport_instance_t * uwb_transport = (uwb_transport_instance_t *)dpl_event_get_arg(ev);
    struct uwb_ccp_instance * ccp = (struct uwb_ccp_instance *)uwb_mac_find_cb_inst_ptr(uwb_transport->dev_inst, UWBEXT_CCP);

    for (uint8_t i = 0; i < 18; i++){
        uint16_t destination_uid = ccp->frames[0]->short_address;  
        struct dpl_mbuf * mbuf;
        if (uwb_transport->config.os_msys_mpool){
            mbuf = dpl_msys_get_pkthdr(sizeof(test), sizeof(uwb_transport_user_header_t));
        }
        else{                           
            mbuf = dpl_mbuf_get_pkthdr(uwb_transport->omp, sizeof(uwb_transport_user_header_t));
        }    
        if (mbuf){
            dpl_mbuf_copyinto(mbuf, 0, &test, sizeof(test));
            uwb_transport_enqueue_tx(uwb_transport, destination_uid, 0xDEAD, mbuf);
            //uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
            //printf("{\"utime\": %lu,\"success\": \"%s:%d\"}\n",utime,__FILE__,__LINE__); 
        }else{
  //          uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
  //          printf("{\"utime\": %lu,\"sys_mpool full\": \"%s:%d\"}\n",utime,__FILE__,__LINE__); 
            break;
        }
    }
}
#endif




int main(int argc, char **argv){
    int rc;

    sysinit();

#if MYNEWT_VAL(UWBCFG_ENABLED)
    /* Register callback for UWB configuration changes */
    struct uwbcfg_cbs uwb_cb = {
        .uc_update = uwb_config_updated_func
    };
    uwbcfg_register(&uwb_cb);
    /* Load config from flash */
    conf_load();
#endif

    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);

    struct uwb_dev * udev = uwb_dev_idx_lookup(0);

#if MYNEWT_VAL(USE_DBLBUFFER)
        /* Make sure to enable double buffring */
        udev->config.dblbuffon_enabled = 1;
        udev->config.rxauto_enable = 0;
        uwb_set_dblrxbuff(udev, true);
#else
        udev->config.dblbuffon_enabled = 0;
        udev->config.rxauto_enable = 1;
        uwb_set_dblrxbuff(udev, false);
#endif

    struct _uwb_transport_instance * uwb_transport = (struct _uwb_transport_instance *)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_TRANSPORT);
    assert(uwb_transport);

    struct _uwb_transport_extension extension = {
        .code = 0xDEAD,
        .uwb_transport = uwb_transport,
        .extension_cb = uwb_transport_cb 
    };
 
    uwb_transport_append_extension(uwb_transport, &extension);

    struct uwb_ccp_instance * ccp = (struct uwb_ccp_instance*)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_CCP);
    assert(ccp);
    
    if ((udev->role & UWB_ROLE_CCP_MASTER) || MYNEWT_VAL(UWB_TRANSPORT_ROLE) == 0) {
        /* Start as clock-master */
        uwb_ccp_start(ccp, CCP_ROLE_MASTER);
    } else {
        uwb_ccp_start(ccp, CCP_ROLE_SLAVE);        
    }

#if MYNEWT_VAL(BLE_ENABLED)
    ble_init(udev->euid);
#endif

#if  MYNEWT_VAL(UWB_DEVICE_0) 
    // Using GPIO5 and GPIO6 to study timing.
    dw1000_gpio5_config_ext_txe( hal_dw1000_inst(0));
    dw1000_gpio6_config_ext_rxe( hal_dw1000_inst(0));
#endif

    uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
    printf("{\"utime\": %lu,\"exec\": \"%s\"}\n",utime,__FILE__); 
    printf("{\"device_id\"=\"%lX\"",udev->device_id);
    printf(",\"panid=\"%X\"",udev->pan_id);
    printf(",\"addr\"=\"%X\"",udev->uid);
    printf(",\"part_id\"=\"%lX\"",(uint32_t)(udev->euid&0xffffffff));
    printf(",\"lot_id\"=\"%lX\"}\n",(uint32_t)(udev->euid>>32));
    printf("{\"utime\": %lu,\"msg\": \"frame_duration = %d usec\"}\n",utime,uwb_phy_frame_duration(udev, sizeof(test) + sizeof(uwb_transport_frame_header_t))); 
    printf("{\"utime\": %lu,\"msg\": \"SHR_duration = %d usec\"}\n",utime,uwb_phy_SHR_duration(udev)); 
    printf("UWB_TRANSPORT_ROLE = %d\n",  MYNEWT_VAL(UWB_TRANSPORT_ROLE));

    tdma_instance_t * tdma = (tdma_instance_t*)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_TDMA);
    assert(tdma);

#if MYNEWT_VAL(CONCURRENT_NRNG)
    struct nrng_instance * nrng = (struct nrng_instance *)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_NRNG);
    assert(nrng);
    /* Slot 0:ccp, 1-160 stream */
    for (uint16_t i = 1; i < MYNEWT_VAL(TDMA_NSLOTS) - 1; i++){
        if(i%16)
            tdma_assign_slot(tdma, range_slot_cb,  i, (void*)nrng);
        else
            tdma_assign_slot(tdma, stream_slot_cb,  i, (void*)uwb_transport);
    }
#else
/* Slot 0:ccp, 1-160 stream */
    for (uint16_t i = 1; i < MYNEWT_VAL(TDMA_NSLOTS) - 1; i++)
            tdma_assign_slot(tdma, stream_slot_cb,  i, (void*)uwb_transport);
#endif

#if MYNEWT_VAL(UWB_TRANSPORT_ROLE) == 1
    for (uint16_t i=0; i < sizeof(test); i++) 
        test[i] = i;
    dpl_callout_init(&stream_callout, dpl_eventq_dflt_get(), stream_timer, uwb_transport);
    dpl_callout_reset(&stream_callout, DPL_TICKS_PER_SEC);
#endif

    while (1) {
        dpl_eventq_run(dpl_eventq_dflt_get());
    }
    assert(0);
    return rc;
}

