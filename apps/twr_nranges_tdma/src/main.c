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
#include <float.h>
#include "sysinit/sysinit.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_bsp.h"
#include "imgmgr/imgmgr.h"

#include <uwb/uwb.h>
#include <uwb/uwb_mac.h>
#include "uwbcfg/uwbcfg.h"
#include <config/config.h>
#include <tdma/tdma.h>

#include <uwb_ccp/uwb_ccp.h>
#include <nrng/nrng.h>
#if MYNEWT_VAL(TIMESCALE)
#include <timescale/timescale.h>
#endif
#if MYNEWT_VAL(UWB_WCS_ENABLED)
#include <uwb_wcs/uwb_wcs.h>
#endif
#if MYNEWT_VAL(SURVEY_ENABLED)
#include <survey/survey.h>
#endif
#if MYNEWT_VAL(NMGR_UWB_ENABLED)
#include <nmgr_uwb/nmgr_uwb.h>
#endif
#if MYNEWT_VAL(BLEPRPH_ENABLED)
#include "bleprph/bleprph.h"
#endif
#if MYNEWT_VAL(UWB_PAN_ENABLED)
#include <uwb_pan/uwb_pan.h>
#include <panmaster/panmaster.h>
#include <bootutil/image.h>

#endif

static bool uwb_config_updated = false;
int
uwb_config_updated_cb()
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
struct uwbcfg_cbs uwb_cb = {
    .uc_update = uwb_config_updated_cb
};


static void nrng_complete_cb(struct dpl_event *ev) {
    assert(ev != NULL);
    assert(dpl_event_get_arg(ev) != NULL);

    hal_gpio_toggle(LED_BLINK_PIN);
    struct nrng_instance * nrng = (struct nrng_instance *) dpl_event_get_arg(ev);
    nrng_frame_t * frame = nrng->frames[(nrng->idx)%nrng->nframes];

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
    if (frame->code == UWB_DATA_CODE_DS_TWR_NRNG_FINAL || frame->code == UWB_DATA_CODE_DS_TWR_NRNG_EXT_FINAL){
        frame->code = UWB_DATA_CODE_DS_TWR_NRNG_END;
    }
}

static struct dpl_event nrng_complete_event;
static bool complete_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    if(inst->fctrl != FCNTL_IEEE_RANGE_16){
        return false;
    }
    dpl_eventq_put(dpl_eventq_dflt_get(), &nrng_complete_event);
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
slot_cb(struct dpl_event * ev){
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

    /* Update config if needed */
    if (uwb_config_updated) {
        uwb_config_updated = false;
        uwb_phy_forcetrxoff(udev);
        uwb_mac_config(udev, NULL);
        uwb_txrf_config(udev, &udev->config.txrf);
        return;
    }

    if (ccp->local_epoch==0 || udev->slot_id == 0xffff) return;

    /* Process any newtmgr packages queued up */
    if (idx > 6 && idx < (tdma->nslots-6) && (idx%4)==0) {
        nmgr_uwb_instance_t *nmgruwb = (nmgr_uwb_instance_t *)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_NMGR_UWB);
        assert(nmgruwb);
        if (uwb_nmgr_process_tx_queue(nmgruwb, tdma_tx_slot_start(tdma, idx))) {
            return;
        }
    }

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
               UWB_DATA_CODE_SS_TWR_NRNG, slot_mask, 0).start_tx_error) {
            uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
            printf("{\"utime\": %lu,\"msg\": \"slot_timer_cb_%d:start_tx_error\"}\n",
                   utime,idx);
        }
    }
}

static void
pan_complete_cb(struct dpl_event * ev)
{
    assert(ev != NULL);
    assert(dpl_event_get_arg(ev) != NULL);
    struct uwb_pan_instance *pan = (struct uwb_pan_instance*) dpl_event_get_arg(ev);

    if (pan->dev_inst->slot_id != 0xffff) {
        uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
        printf("{\"utime\": %lu,\"msg\": \"slot_id = %d\"}\n", utime, pan->dev_inst->slot_id);
        printf("{\"utime\": %lu,\"msg\": \"euid16 = 0x%X\"}\n", utime, pan->dev_inst->my_short_address);
    }
}

/* This function allows the ccp to compensate for the time of flight
 * from the master anchor to the current anchor.
 * Ideally this should use a map generated and make use of the euid in case
 * the ccp packet is relayed through another node.
 */
static uint32_t
tof_comp_cb(uint16_t short_addr)
{
    float x = MYNEWT_VAL(UWB_CCP_TOF_COMP_LOCATION_X);
    float y = MYNEWT_VAL(UWB_CCP_TOF_COMP_LOCATION_Y);
    float z = MYNEWT_VAL(UWB_CCP_TOF_COMP_LOCATION_Z);
    float dist_in_meters = sqrtf(x*x+y*y+z*z);
#ifdef VERBOSE
    printf("d=%dm, %ld dwunits\n", (int)dist_in_meters,
           (uint32_t)(dist_in_meters/uwb_rng_tof_to_meters(1.0)));
#endif
    return dist_in_meters/uwb_rng_tof_to_meters(1.0);
}


int main(int argc, char **argv){
    int rc;

    sysinit();
    uwbcfg_register(&uwb_cb);
    conf_load();

    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);

    struct uwb_dev * udev = uwb_dev_idx_lookup(0);
    udev->config.rxauto_enable = false;
    udev->config.dblbuffon_enabled = false;
    uwb_set_dblrxbuff(udev, udev->config.dblbuffon_enabled);

    struct nrng_instance* nrng = (struct nrng_instance*)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_NRNG);
    assert(nrng);

    dpl_event_init(&nrng_complete_event, nrng_complete_cb, nrng);

    struct uwb_mac_interface cbs = (struct uwb_mac_interface){
        .id = UWBEXT_APP0,
        .inst_ptr = nrng,
        .complete_cb = complete_cb
    };

    uwb_mac_append_interface(udev, &cbs);
    udev->slot_id = 0xffff;
#if MYNEWT_VAL(BLEPRPH_ENABLED)
    ble_init(udev->euid);
#endif
    struct uwb_ccp_instance *ccp = (struct uwb_ccp_instance*)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_CCP);
    assert(ccp);
    struct uwb_pan_instance *pan = (struct uwb_pan_instance*)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_PAN);
    assert(pan);
    struct uwb_rng_instance* rng = (struct uwb_rng_instance*)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_RNG);
    assert(rng);

    if (udev->role&UWB_ROLE_CCP_MASTER) {
        /* Start as clock-master */
        uwb_ccp_start(ccp, CCP_ROLE_MASTER);
    } else {
        uwb_ccp_start(ccp, CCP_ROLE_SLAVE);
        uwb_ccp_set_tof_comp_cb(ccp, tof_comp_cb);
    }

    if (udev->role&UWB_ROLE_PAN_MASTER) {
        /* As pan-master, first lookup our address and slot_id */
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
        uwb_pan_set_postprocess(pan, pan_complete_cb);
        network_role_t role = (udev->role&UWB_ROLE_ANCHOR)?
            NETWORK_ROLE_ANCHOR : NETWORK_ROLE_TAG;
        uwb_pan_start(pan, UWB_PAN_ROLE_RELAY, role);
    }

    uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
    printf("{\"utime\": %lu,\"exec\": \"%s\"}\n",utime,__FILE__);
    printf("{\"device_id\":\"%lX\"",udev->device_id);
    printf(",\"panid\":\"%X\"",udev->pan_id);
    printf(",\"addr\":\"%X\"",udev->uid);
    printf(",\"part_id\":\"%lX\"",(uint32_t)(udev->euid&0xffffffff));
    printf(",\"lot_id\":\"%lX\"}\n",(uint32_t)(udev->euid>>32));
    printf("{\"utime\": %lu,\"msg\": \"frame_duration = %d usec\"}\n",utime, uwb_phy_frame_duration(udev, sizeof(twr_frame_final_t)));
    printf("{\"utime\": %lu,\"msg\": \"SHR_duration = %d usec\"}\n",utime, uwb_phy_SHR_duration(udev));
    printf("{\"utime\": %lu,\"msg\": \"holdoff = %d usec\"}\n",utime,(uint16_t)ceilf(uwb_dwt_usecs_to_usecs(rng->config.tx_holdoff_delay)));

    /* Pan is slots 1&2 */
    tdma_instance_t * tdma = (tdma_instance_t*)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_TDMA);
    assert(tdma);
    tdma_assign_slot(tdma, uwb_pan_slot_timer_cb, 1, (void*)pan);
    tdma_assign_slot(tdma, uwb_pan_slot_timer_cb, 2, (void*)pan);

#if MYNEWT_VAL(SURVEY_ENABLED)
    survey_instance_t *survey = (survey_instance_t*)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_SURVEY);

    tdma_assign_slot(tdma, survey_slot_range_cb, MYNEWT_VAL(SURVEY_RANGE_SLOT), (void*)survey);
    tdma_assign_slot(tdma, survey_slot_broadcast_cb, MYNEWT_VAL(SURVEY_BROADCAST_SLOT), (void*)survey);
    for (uint16_t i = 6; i < MYNEWT_VAL(TDMA_NSLOTS); i++)
#else
    for (uint16_t i = 3; i < MYNEWT_VAL(TDMA_NSLOTS); i++)
#endif
        tdma_assign_slot(tdma, slot_cb, i, (void*)nrng);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    assert(0);
    return rc;
}
