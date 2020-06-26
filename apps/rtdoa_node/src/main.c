/**
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

#include <uwb/uwb.h>
#include <uwb/uwb_mac.h>
#include <uwb/uwb_ftypes.h>
#include "dw1000/dw1000_dev.h"
#include "dw1000/dw1000_hal.h"
#include "uwbcfg/uwbcfg.h"
#include <config/config.h>

#if MYNEWT_VAL(NMGR_UWB_ENABLED)
#include <nmgr_uwb/nmgr_uwb.h>
#endif

#include <tdma/tdma.h>
#include <uwb_ccp/uwb_ccp.h>
#include <uwb_wcs/uwb_wcs.h>
#include <timescale/timescale.h>
#if MYNEWT_VAL(UWB_RNG_ENABLED)
#include <uwb_rng/uwb_rng.h>
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


#if MYNEWT_VAL(UWB_PAN_ENABLED)
#include <uwb_pan/uwb_pan.h>

#include "panmaster/panmaster.h"

#endif

static bool uwb_config_updated = false;
int
uwb_config_upd_cb()
{
    /* Workaround in case we're stuck waiting for ccp with the
     * wrong radio settings */
    struct uwb_dev * inst = uwb_dev_idx_lookup(0);
    struct uwb_ccp_instance *ccp = (struct uwb_ccp_instance*)uwb_mac_find_cb_inst_ptr(inst, UWBEXT_CCP);
    if (dpl_sem_get_count(&ccp->sem) == 0 || !ccp->status.valid) {
        uwb_mac_config(inst, NULL);
        uwb_txrf_config(inst, &inst->config.txrf);
        if (dpl_sem_get_count(&ccp->sem) == 0) {
            uwb_start_rx(inst);
        }
        return 0;
    }

    uwb_config_updated = true;
    return 0;
}
struct uwbcfg_cbs uwb_cb = {
    .uc_update = uwb_config_upd_cb
};


/**
 * @fn nrng_slot_timer_cb(struct dpl_event * ev)
 *
 * @brief Node to Node ranging for tof timing compensation
 *
 */
static void
nrng_slot_timer_cb(struct dpl_event *ev)
{
    assert(ev);
    tdma_slot_t * slot = (tdma_slot_t *) dpl_event_get_arg(ev);;
    tdma_instance_t * tdma = slot->parent;
    struct uwb_dev * inst = tdma->dev_inst;
    struct uwb_ccp_instance * ccp = tdma->ccp;
    struct nrng_instance * nrng = (struct nrng_instance *) uwb_mac_find_cb_inst_ptr(inst, UWBEXT_NRNG);

    uint16_t idx = slot->idx;

    /* Avoid colliding with the ccp */
    if (dpl_sem_get_count(&ccp->sem) == 0 || idx == 0xffff) {
        return;
    }

    hal_gpio_write(LED_BLINK_PIN, 1);

    uint16_t anchor_rng_initiator = ccp->seq_num % 8;
    if (anchor_rng_initiator == inst->slot_id) {
        uint64_t dx_time = tdma_tx_slot_start(tdma, idx) & 0xFFFFFFFFFE00UL;
        uint32_t slot_mask = 0xFFFF;

        if(nrng_request_delay_start(nrng, UWB_BROADCAST_ADDRESS, dx_time,
                                    UWB_DATA_CODE_SS_TWR_NRNG, slot_mask, 0).start_tx_error){
            /* Do nothing */
        }
    } else {
        uwb_set_delay_start(inst, tdma_rx_slot_start(tdma, idx));
        uint16_t timeout = uwb_phy_frame_duration(inst, sizeof(nrng_request_frame_t))
            + nrng->config.rx_timeout_delay;

        uwb_set_rx_timeout(inst, timeout + 0x100);
        nrng_listen(nrng, UWB_BLOCKING);
    }
    hal_gpio_write(LED_BLINK_PIN, 0);
}

static void
nrng_complete_cb(struct dpl_event *ev)
{
    assert(ev != NULL);
    assert(dpl_event_get_arg(ev));

    struct nrng_instance * nrng = (struct nrng_instance *) dpl_event_get_arg(ev);
    nrng_frame_t * frame = nrng->frames[(nrng->idx)%nrng->nframes];

    for (int i=0;i<nrng->nframes;i++) {
        frame = nrng->frames[(nrng->idx + i)%nrng->nframes];
        uint16_t dst_addr = frame->dst_address;
        if (frame->code != UWB_DATA_CODE_SS_TWR_NRNG_FINAL || frame->seq_num != nrng->seq_num) {
            continue;
        }

        float tof = nrng_twr_to_tof_frames(nrng->dev_inst, frame, frame);
        // float rssi = dw1000_calc_rssi(inst, &frame->diag);
        tofdb_set_tof(dst_addr, tof);
    }
}

static struct dpl_event slot_event;
static bool complete_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    if(inst->fctrl != FCNTL_IEEE_RANGE_16){
        return false;
    }
    dpl_eventq_put(dpl_eventq_dflt_get(), &slot_event);
    return true;
}

/**
 * @fn rtdoa_slot_timer_cb(struct dpl_event * ev)
 *
 * @brief RTDoA Emission slot
 *
 */
static void
rtdoa_slot_timer_cb(struct dpl_event *ev)
{
    assert(ev);
    tdma_slot_t * slot = (tdma_slot_t *) dpl_event_get_arg(ev);
    uint16_t idx = slot->idx;
    // printf("rtdoa %02d\n", idx);
    tdma_instance_t * tdma = slot->parent;
    assert(tdma);
    struct uwb_ccp_instance * ccp = tdma->ccp;
    assert(ccp);
    struct uwb_dev * inst = tdma->dev_inst;
    assert(inst);
    struct rtdoa_instance* rtdoa = (struct rtdoa_instance*)slot->arg;
    assert(rtdoa);

    /* Avoid colliding with the ccp */
    if (dpl_sem_get_count(&ccp->sem) == 0) {
        return;
    }

    if (uwb_config_updated) {
        uwb_mac_config(inst, NULL);
        uwb_txrf_config(inst, &inst->config.txrf);
        uwb_config_updated = false;
        return;
    }

    /* See if there's anything to send, if so finish early */
    nmgr_uwb_instance_t *nmgruwb = (nmgr_uwb_instance_t*)uwb_mac_find_cb_inst_ptr(tdma->dev_inst, UWBEXT_NMGR_UWB);
    assert(nmgruwb);
    if (uwb_nmgr_process_tx_queue(nmgruwb, tdma_tx_slot_start(tdma, idx)) == true) {
        return;
    }

    if (inst->role & UWB_ROLE_CCP_MASTER) {
        uint64_t dx_time = tdma_tx_slot_start(tdma, idx) & 0xFFFFFFFFFE00UL;

        if(rtdoa_request(rtdoa, dx_time).start_tx_error) {
            /* Do nothing */
            printf("rtdoa_start_err\n");
        }
    } else {
        uint64_t dx_time = tdma_rx_slot_start(tdma, idx);
        if(rtdoa_listen(rtdoa, UWB_BLOCKING, dx_time, 3*ccp->period/tdma->nslots/4).start_rx_error) {
            printf("#rse\n");
        }
    }
}

static void
nmgr_slot_timer_cb(struct dpl_event * ev)
{
    assert(ev);
    tdma_slot_t * slot = (tdma_slot_t *) dpl_event_get_arg(ev);
    tdma_instance_t * tdma = slot->parent;
    struct uwb_ccp_instance * ccp = tdma->ccp;
    uint16_t idx = slot->idx;
    nmgr_uwb_instance_t * nmgruwb = (nmgr_uwb_instance_t *) slot->arg;
    assert(nmgruwb);
    // printf("idx %02d nmgr\n", idx);

    /* Avoid colliding with the ccp */
    if (dpl_sem_get_count(&ccp->sem) == 0) {
        return;
    }

    uint16_t timeout = 3*ccp->period/tdma->nslots/4;
    if (uwb_nmgr_process_tx_queue(nmgruwb, tdma_tx_slot_start(tdma, idx)) == false) {
        nmgr_uwb_listen(nmgruwb, UWB_BLOCKING, tdma_rx_slot_start(tdma, idx), timeout);
    }
}


static void
tdma_allocate_slots(tdma_instance_t * tdma)
{
    uint16_t i;
    struct uwb_dev * inst = tdma->dev_inst;

    /* Pan is slot 1 */
    struct uwb_pan_instance *pan = (struct uwb_pan_instance*)uwb_mac_find_cb_inst_ptr(inst, UWBEXT_PAN);
    assert(pan);
    tdma_assign_slot(tdma, uwb_pan_slot_timer_cb, 1, (void*)pan);

    /* anchor-to-anchor range slot is 31 */
    struct nrng_instance * nrng = (struct nrng_instance *)uwb_mac_find_cb_inst_ptr(inst, UWBEXT_NRNG);
    assert(nrng);
    tdma_assign_slot(tdma, nrng_slot_timer_cb, 31, (void*)nrng);

    nmgr_uwb_instance_t *nmgruwb = (nmgr_uwb_instance_t*)uwb_mac_find_cb_inst_ptr(inst, UWBEXT_NMGR_UWB);
    assert(nmgruwb);
    struct rtdoa_instance* rtdoa = (struct rtdoa_instance*)uwb_mac_find_cb_inst_ptr(inst, UWBEXT_RTDOA);
    assert(rtdoa);

    for (i=2;i < MYNEWT_VAL(TDMA_NSLOTS);i++) {
        if (i==31) {
            continue;
        }
        if (i%12==0) {
            tdma_assign_slot(tdma, nmgr_slot_timer_cb, i, (void*)nmgruwb);
        } else {
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

    struct uwb_mac_interface cbs = (struct uwb_mac_interface){
        .id =  UWBEXT_APP0,
        .complete_cb = complete_cb
    };

    struct uwb_dev *udev = uwb_dev_idx_lookup(0);
    uwb_mac_append_interface(udev, &cbs);
    struct nrng_instance * nrng = (struct nrng_instance *)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_NRNG);
    assert(nrng);
    dpl_event_init(&slot_event, nrng_complete_cb, nrng);

    udev->config.rxauto_enable = 0;
    udev->config.trxoff_enable = 1;
    udev->config.rxdiag_enable = 1;
    udev->config.sleep_enable = 1;
    udev->config.dblbuffon_enabled = 0;
    uwb_set_dblrxbuff(udev, false);

    udev->slot_id = 0xffff;

    ble_init(udev->my_long_address);

    struct uwb_ccp_instance *ccp = (struct uwb_ccp_instance *) uwb_mac_find_cb_inst_ptr(udev, UWBEXT_CCP);
    assert(ccp);
    struct uwb_pan_instance *pan = (struct uwb_pan_instance *) uwb_mac_find_cb_inst_ptr(udev, UWBEXT_PAN);
    assert(pan);

    if (udev->role & UWB_ROLE_CCP_MASTER) {
        printf("{\"role\":\"ccp_master\"}\n");
        uwb_ccp_start(ccp, CCP_ROLE_MASTER);

        struct image_version fw_ver;
        struct panmaster_node *node;
        panmaster_idx_find_node(udev->euid, NETWORK_ROLE_ANCHOR, &node);
        assert(node);
        imgr_my_version(&fw_ver);
        node->fw_ver.iv_major = fw_ver.iv_major;
        node->fw_ver.iv_minor = fw_ver.iv_minor;
        node->fw_ver.iv_revision = fw_ver.iv_revision;
        node->fw_ver.iv_build_num = fw_ver.iv_build_num;
        udev->my_short_address = node->addr;
        udev->slot_id = node->slot_id;
        panmaster_postprocess();
        uwb_pan_start(pan, UWB_PAN_ROLE_MASTER, NETWORK_ROLE_ANCHOR);
    } else {
        uwb_ccp_start(ccp, CCP_ROLE_RELAY);
        uwb_pan_start(pan, UWB_PAN_ROLE_RELAY, NETWORK_ROLE_ANCHOR);
    }
    printf("{\"device_id\":\"%lX\"",udev->device_id);
    printf(",\"panid\":\"%X\"",udev->pan_id);
    printf(",\"addr\":\"%X\"",udev->uid);
    printf(",\"part_id\":\"%lX\"",(uint32_t)(udev->euid&0xffffffff));
    printf(",\"lot_id\":\"%lX\"}\n",(uint32_t)(udev->euid>>32));

    tdma_instance_t * tdma = (tdma_instance_t*)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_TDMA);
    assert(tdma);
    tdma_allocate_slots(tdma);

#if MYNEWT_VAL(NCBWIFI_ESP_PASSTHROUGH)
    hal_bsp_esp_bypass(true);
#endif

    while (1) {
        dpl_eventq_run(dpl_eventq_dflt_get());
    }
    assert(0);
    return rc;
}
