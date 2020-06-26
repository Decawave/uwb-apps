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
#include <uwb/uwb_mac.h>
#include <uwb_rng/uwb_rng.h>
#include <config/config.h>
#include "uwbcfg/uwbcfg.h"

#if MYNEWT_VAL(TDMA_ENABLED)
#include <tdma/tdma.h>
#endif
#if MYNEWT_VAL(UWB_CCP_ENABLED)
#include <uwb_ccp/uwb_ccp.h>
#endif
#if MYNEWT_VAL(CIR_ENABLED)
#include <cir/cir.h>
#endif

//#define DIAGMSG(s,u) printf(s,u)
#ifndef DIAGMSG
#define DIAGMSG(s,u)
#endif

static bool uwb_config_updated = false;
static void slot_complete_cb(struct dpl_event *ev);
static bool cir_complete_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs);
static bool complete_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs);

#define ANTENNA_SEPERATION 0.0205f

static triadf_t g_angle = {0};
static bool
cir_complete_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    if (inst->fctrl != FCNTL_IEEE_RANGE_16 &&
        inst->fctrl != (FCNTL_IEEE_RANGE_16|UWB_FCTRL_ACK_REQUESTED)) {
        return false;
    }

#if MYNEWT_VAL(UWB_DEVICE_0) && MYNEWT_VAL(UWB_DEVICE_1)
    struct uwb_dev * udev[] = {
        uwb_dev_idx_lookup(0),
        uwb_dev_idx_lookup(1)
    };
    struct cir_instance * cir[] = {
        udev[0]->cir,
        udev[1]->cir
    };

    twr_frame_t * frame0 = (twr_frame_t *) udev[0]->rxbuf;
    twr_frame_t * frame1 = (twr_frame_t *) udev[1]->rxbuf;

    struct uwb_rng_instance * rng = (struct uwb_rng_instance*)cbs->inst_ptr;
    twr_frame_t * frame = rng->frames[rng->idx_current];
    if ((cir[0]->status.valid && cir[1]->status.valid) && (frame0->seq_num == frame1->seq_num)){
#if MYNEWT_VAL(CIR_ENABLED)
        float pd = cir_get_pdoa(cir[1], cir[0]);
#endif
        frame->local.spherical.azimuth = uwb_calc_aoa(pd, inst->config.channel, ANTENNA_SEPERATION);
   }
#endif

    return true;
}

/*!
 * @fn complete_cb
 *
 * @brief This callback is part of the  struct uwb_mac_interface extension interface and invoked of the completion of a range request.
 * The struct uwb_mac_interface is in the interrupt context and is used to schedule events an event queue. Processing should be kept
 * to a minimum giving the interrupt context. All algorithms activities should be deferred to a thread on an event queue.
 * The callback should return true if and only if it can determine if it is the sole recipient of this event.
 *
 * NOTE: The MAC extension interface is a link-list of callbacks, subsequent callbacks on the list will be not be called in the
 * event of returning true.
 *
 * @param inst  - struct uwb_dev *
 * @param cbs   - struct uwb_mac_interface *
 *
 * output parameters
 *
 * returns bool
 */
/* The timer callout */
static struct dpl_event slot_event = {0};
static bool
complete_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    if (inst->fctrl != FCNTL_IEEE_RANGE_16 &&
        inst->fctrl != (FCNTL_IEEE_RANGE_16|UWB_FCTRL_ACK_REQUESTED)) {
        return false;
    }

    struct uwb_rng_instance * rng = (struct uwb_rng_instance*)cbs->inst_ptr;
    twr_frame_t * frame = rng->frames[rng->idx_current];

    if (inst->capabilities.single_receiver_pdoa) {
        float pd = uwb_calc_pdoa(inst, inst->rxdiag);
        g_angle.azimuth = uwb_calc_aoa(
            pd, inst->config.channel, ANTENNA_SEPERATION);
    }
#if MYNEWT_VAL(AOA_ANGLE_INVERT)
    frame->local.spherical.azimuth = -g_angle.azimuth;
#else
    frame->local.spherical.azimuth = g_angle.azimuth;
#endif
    frame->local.spherical.zenith = g_angle.zenith;

    dpl_eventq_put(dpl_eventq_dflt_get(), &slot_event);
    return true;
}

/*!
 * @fn slot_complete_cb(struct os_event * ev)
 *
 * @brief In the example this function represents the event context processing of the received range request.
 * In this case, a JSON string is constructed and written to stdio. See the ./apps/matlab or ./apps/python folders for examples on
 * how to parse and render these results.
 *
 * input parameters
 * @param inst - struct os_event *
 * output parameters
 * returns none
 */

static void
slot_complete_cb(struct dpl_event *ev)
{
    assert(ev != NULL);

    hal_gpio_toggle(LED_BLINK_PIN);
}


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
#if MYNEWT_VAL(UWB_DEVICE_1)
        uwb_mac_config(uwb_dev_idx_lookup(1), NULL);
        uwb_txrf_config(uwb_dev_idx_lookup(1), &uwb_dev_idx_lookup(1)->config.txrf);
#endif
        /* Prepare for autoack */
        if (udev->config.rx.frameFilter) {
            uwb_set_autoack(udev, true);
            uwb_set_autoack_delay(udev, 0);
        } else {
            uwb_set_autoack(udev, false);
        }
        uwb_start_rx(udev);
        return 0;
    }

    uwb_config_updated = true;
    return 0;
}

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

static void
slot_cb(struct dpl_event * ev)
{
    static uint16_t timeout = 0;
    assert(ev);

    tdma_slot_t * slot = (tdma_slot_t *) dpl_event_get_arg(ev);
    tdma_instance_t * tdma = slot->parent;
    struct uwb_ccp_instance *ccp = tdma->ccp;
    struct uwb_dev *inst = tdma->dev_inst;
    uint16_t idx = slot->idx;
    struct uwb_rng_instance * rng = (struct uwb_rng_instance*)slot->arg;
    g_angle.azimuth = g_angle.zenith = NAN;
    //printf("idx%d\n", idx);

    /* Avoid colliding with the ccp in case we've got out of sync */
    if (dpl_sem_get_count(&ccp->sem) == 0) {
        return;
    }

    if (uwb_config_updated) {
        uwb_mac_config(inst, NULL);
        uwb_txrf_config(inst, &inst->config.txrf);
#if MYNEWT_VAL(UWB_DEVICE_1)
        uwb_mac_config(uwb_dev_idx_lookup(1), NULL);
        uwb_txrf_config(uwb_dev_idx_lookup(1), &uwb_dev_idx_lookup(1)->config.txrf);
#endif
        uwb_config_updated = false;
        /* Prepare for autoack */
        if (inst->config.rx.frameFilter) {
            uwb_set_autoack(inst, true);
            uwb_set_autoack_delay(inst, 0);
        } else {
            uwb_set_autoack(inst, false);
        }
        timeout = 0;
        return;
    }

    /* Only recalculate timeout if needed */
    if (!timeout) {
        timeout = uwb_usecs_to_dwt_usecs(uwb_phy_frame_duration(inst, sizeof(ieee_rng_request_frame_t)))
            + rng->config.rx_timeout_delay;
        printf("# timeout set to: %d %d = %d\n",
               uwb_phy_frame_duration(inst, sizeof(ieee_rng_request_frame_t)),
               rng->config.rx_timeout_delay, timeout);
    }

#if MYNEWT_VAL(UWB_DEVICE_0) && MYNEWT_VAL(UWB_DEVICE_1)
{
    struct uwb_dev * inst = uwb_dev_idx_lookup(0);
    uwb_set_delay_start(inst, tdma_rx_slot_start(tdma, idx));
    uwb_set_rx_timeout(inst, timeout);
    cir_enable(uwb_dev_idx_lookup(0)->cir, true);
    uwb_set_rxauto_disable(inst, true);
    uwb_start_rx(inst);  // RX enabled but frames handled as unsolicited inbound
}
{
    struct uwb_dev * inst = uwb_dev_idx_lookup(1);
    struct uwb_rng_instance * rng = (struct uwb_rng_instance*)uwb_mac_find_cb_inst_ptr(inst, UWBEXT_RNG);
    cir_enable(uwb_dev_idx_lookup(1)->cir, true);
    uwb_rng_listen_delay_start(rng, tdma_rx_slot_start(tdma, idx), timeout, UWB_BLOCKING);
}
#else
    uwb_rng_listen_delay_start(rng, tdma_rx_slot_start(tdma, idx), timeout, UWB_BLOCKING);
#endif

}



int main(int argc, char **argv){
    int rc;

    sysinit();
    /* Register callback for UWB configuration changes */
    struct uwbcfg_cbs uwb_cb = {
        .uc_update = uwb_config_updated_func
    };
    uwbcfg_register(&uwb_cb);
    /* Load config from flash */
    conf_load();

    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);

#if MYNEWT_VAL(UWB_DEVICE_0) && MYNEWT_VAL(UWB_DEVICE_1)
{
    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
    dw1000_set_dblrxbuff(inst, false);
}
    struct uwb_dev * udev = uwb_dev_idx_lookup(1);
    dw1000_dev_instance_t * inst = hal_dw1000_inst(1);
    dw1000_set_dblrxbuff(inst, false);
    struct uwb_rng_instance * rng = (struct uwb_rng_instance *)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_RNG);
    assert(rng);
#elif  MYNEWT_VAL(UWB_DEVICE_0) && !MYNEWT_VAL(UWB_DEVICE_1)
    struct uwb_dev * udev = uwb_dev_idx_lookup(0);
    uwb_set_dblrxbuff(udev, false);
    struct uwb_rng_instance * rng = (struct uwb_rng_instance *)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_RNG);
    assert(rng);
#endif


    struct uwb_mac_interface cbs = (struct uwb_mac_interface){
        .id =  UWBEXT_APP0,
        .inst_ptr = rng,
        .complete_cb = complete_cb,
        .cir_complete_cb = cir_complete_cb
    };
    uwb_mac_append_interface(udev, &cbs);

    struct uwb_ccp_instance *ccp = (struct uwb_ccp_instance*)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_CCP);
    assert(ccp);

    if (udev->role & UWB_ROLE_CCP_MASTER) {
        /* Start as clock-master */
        uwb_ccp_start(ccp, CCP_ROLE_MASTER);
    } else {
        uwb_ccp_start(ccp, CCP_ROLE_SLAVE);
    }

#if MYNEWT_VAL(UWB_DEVICE_0) && MYNEWT_VAL(UWB_DEVICE_1)
    /* Sync clocks if available */
    if (uwb_sync_to_ext_clock(udev).ext_sync == 1) {
        printf("{\"ext_sync\"=\"%d\"}\n", udev->status.ext_sync);
    }
#endif

    uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
    printf("{\"utime\": %lu,\"exec\": \"%s\"}\n",utime,__FILE__);
    printf("{\"device_id\"=\"%lX\"",udev->device_id);
    printf(",\"role=\"%X\"",udev->role);
    printf(",\"panid=\"%X\"",udev->pan_id);
    printf(",\"addr\"=\"%X\"",udev->uid);
    printf(",\"part_id\"=\"%lX\"",(uint32_t)(udev->euid&0xffffffff));
    printf(",\"lot_id\"=\"%lX\"}\n",(uint32_t)(udev->euid>>32));
    printf("{\"utime\": %lu,\"msg\": \"frame_duration = %d usec\"}\n",utime,uwb_phy_frame_duration(udev, sizeof(twr_frame_final_t)));
    printf("{\"utime\": %lu,\"msg\": \"SHR_duration = %d usec\"}\n",utime,uwb_phy_SHR_duration(udev));
    printf("{\"utime\": %lu,\"msg\": \"holdoff = %d usec\"}\n",utime,(uint16_t)ceilf(uwb_dwt_usecs_to_usecs(rng->config.tx_holdoff_delay)));

    tdma_instance_t * tdma = (tdma_instance_t*)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_TDMA);
    assert(tdma);

#if  MYNEWT_VAL(UWB_DEVICE_0) && MYNEWT_VAL(UWB_DEVICE_1)
    // Using GPIO5 and GPIO6 to study timing.
    dw1000_gpio5_config_ext_txe( hal_dw1000_inst(0));
    dw1000_gpio5_config_ext_txe( hal_dw1000_inst(1));
    dw1000_gpio6_config_ext_rxe( hal_dw1000_inst(0));
    dw1000_gpio6_config_ext_rxe( hal_dw1000_inst(1));
#endif

#if MYNEWT_VAL(BLE_ENABLED)
    ble_init(udev->euid);
#endif
    dpl_event_init(&slot_event, slot_complete_cb, rng);

    /* Slot 0:ccp, 1+ twr */
    for (uint16_t i = 1; i < MYNEWT_VAL(TDMA_NSLOTS); i++)
        tdma_assign_slot(tdma, slot_cb,  i, (void*)rng);

#if MYNEWT_VAL(RNG_VERBOSE) > 1
    udev->config.rxdiag_enable = 1;
#else
    udev->config.rxdiag_enable = 0;
#endif

    while (1) {
        dpl_eventq_run(dpl_eventq_dflt_get());
    }
    assert(0);
    return rc;
}
