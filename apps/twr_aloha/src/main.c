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

#include <uwb/uwb.h>
#include <uwb_rng/uwb_rng.h>
#include <config/config.h>
#include <uwbcfg/uwbcfg.h>
#include <uwb_rng/rng_encode.h>
//#define DIAGMSG(s,u) printf(s,u)
#ifndef DIAGMSG
#define DIAGMSG(s,u)
#endif
#if MYNEWT_VAL(CIR_ENABLED)
#include <cir/cir.h>
#endif

static void slot_complete_cb(struct dpl_event *ev);

/*!
 * @fn complete_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
 *
 * @brief This callback is part of the  struct uwb_mac_interface extension interface and invoked of the completion of a range request.
 * The struct uwb_mac_interface is in the interrupt context and is used to schedule events an event queue. Processing should be kept
 * to a minimum giving the interrupt context. All algorithms activities should be deferred to a thread on an event queue.
 * The callback should return true if and only if it can determine if it is the sole recipient of this event.
 *
 * NOTE: The MAC extension interface is a link-list of callbacks, subsequent callbacks on the list will be not be called in the
 * event of returning true.
 *
 * @param inst  - dw1000_dev_instance_t *
 * @param cbs   - struct uwb_mac_interface *
 *
 * output parameters
 *
 * returns bool
 */
/* The timer callout */

static struct dpl_event slot_event = {0};
static struct os_callout tx_callout;
static uint16_t g_idx_latest;

static bool
complete_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    if(inst->fctrl != FCNTL_IEEE_RANGE_16){
        return false;
    }
    struct uwb_rng_instance * rng = (struct uwb_rng_instance*)cbs->inst_ptr;
    g_idx_latest = (rng->idx)%rng->nframes; // Store valid frame pointer
    dpl_eventq_put(dpl_eventq_dflt_get(), &slot_event);
    return true;
}

static bool
rx_timeout_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    struct uwb_rng_instance * rng = (struct uwb_rng_instance*)cbs->inst_ptr;
    if (inst->role&UWB_ROLE_ANCHOR) {
        uwb_phy_forcetrxoff(inst);
        uwb_rng_listen(rng, 0xfffff, UWB_NONBLOCKING);
    } else {
        /* Do nothing */
    }
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

static void slot_complete_cb(struct dpl_event *ev)
{
    assert(ev != NULL);

    hal_gpio_toggle(LED_BLINK_PIN);

    struct uwb_rng_instance * rng = (struct uwb_rng_instance *)dpl_event_get_arg(ev);
    struct uwb_dev * inst = rng->dev_inst;

    if (inst->role&UWB_ROLE_ANCHOR) {
        uwb_rng_listen(rng, 0xfffff, UWB_NONBLOCKING);
    }
}

static void
uwb_ev_cb(struct os_event *ev)
{
    struct uwb_rng_instance * rng = (struct uwb_rng_instance *)ev->ev_arg;
    struct uwb_dev * inst = rng->dev_inst;

    if (inst->role&UWB_ROLE_ANCHOR) {
        if(dpl_sem_get_count(&rng->sem) == 1){
            uwb_rng_listen(rng, 0xfffff, UWB_NONBLOCKING);
        }
    } else {
        int mode_v[8] = {0}, mode_i=0, mode=-1;
        static int last_used_mode = 0;
#if MYNEWT_VAL(TWR_SS_ENABLED)
        mode_v[mode_i++] = UWB_DATA_CODE_SS_TWR;
#endif
#if MYNEWT_VAL(TWR_SS_EXT_ENABLED)
        mode_v[mode_i++] = UWB_DATA_CODE_SS_TWR_EXT;
#endif
#if MYNEWT_VAL(TWR_SS_ACK_ENABLED)
        mode_v[mode_i++] = UWB_DATA_CODE_SS_TWR_ACK;
#endif
#if MYNEWT_VAL(TWR_DS_ENABLED)
        mode_v[mode_i++] = UWB_DATA_CODE_DS_TWR;
#endif
#if MYNEWT_VAL(TWR_DS_EXT_ENABLED)
        mode_v[mode_i++] = UWB_DATA_CODE_DS_TWR_EXT;
#endif
        if (++last_used_mode >= mode_i) last_used_mode=0;
        mode = mode_v[last_used_mode];
        /* Uncomment the next line to force the range mode */
        // mode = UWB_DATA_CODE_SS_TWR;
        if (mode>0) {
            uwb_rng_request(rng, MYNEWT_VAL(ANCHOR_ADDRESS), mode);
        }
    }
    os_callout_reset(&tx_callout, OS_TICKS_PER_SEC/60);
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
    struct uwb_dev *inst = uwb_dev_idx_lookup(0);
    struct uwb_rng_instance * rng = (struct uwb_rng_instance*)uwb_mac_find_cb_inst_ptr(
        inst, UWBEXT_RNG);
    assert(rng);
    uwb_mac_config(inst, NULL);
    uwb_txrf_config(inst, &inst->config.txrf);

    if (inst->role&UWB_ROLE_ANCHOR) {
        uwb_phy_forcetrxoff(inst);
        uwb_rng_listen(rng, 0xfffff, UWB_NONBLOCKING);
    } else {
        /* Do nothing */
    }
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

    struct uwb_dev *udev = uwb_dev_idx_lookup(0);
    struct uwb_rng_instance * rng = (struct uwb_rng_instance*)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_RNG);
    assert(rng);
    struct uwb_mac_interface cbs = (struct uwb_mac_interface){
        .id = UWBEXT_APP0,
        .inst_ptr = rng,
        .complete_cb = complete_cb,
        .rx_timeout_cb = rx_timeout_cb,
   };
    uwb_mac_append_interface(udev, &cbs);

    uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
    printf("{\"utime\": %lu,\"exec\": \"%s\"}\n",utime,__FILE__);
    printf("{\"device_id\"=\"%lX\"",udev->device_id);
    printf(",\"panid=\"%X\"",udev->pan_id);
    printf(",\"addr\"=\"%X\"",udev->uid);
    printf(",\"part_id\"=\"%lX\"",(uint32_t)(udev->euid&0xffffffff));
    printf(",\"lot_id\"=\"%lX\"}\n",(uint32_t)(udev->euid>>32));
    printf("{\"utime\": %lu,\"msg\": \"frame_duration = %d usec\"}\n",utime, uwb_phy_frame_duration(udev, sizeof(twr_frame_final_t)));
    printf("{\"utime\": %lu,\"msg\": \"SHR_duration = %d usec\"}\n",utime,uwb_phy_SHR_duration(udev));
    printf("{\"utime\": %lu,\"msg\": \"holdoff = %d usec\"}\n",utime,(uint16_t)ceilf(uwb_dwt_usecs_to_usecs(rng->config.tx_holdoff_delay)));

#if MYNEWT_VAL(TWR_SS_ACK_ENABLED)
    uwb_set_autoack(udev, true);
    uwb_set_autoack_delay(udev, 12);
#endif

    os_callout_init(&tx_callout, os_eventq_dflt_get(), uwb_ev_cb, rng);
    os_callout_reset(&tx_callout, OS_TICKS_PER_SEC/25);

    dpl_event_init(&slot_event, slot_complete_cb, rng);

    if ((udev->role&UWB_ROLE_ANCHOR)) {
        udev->my_short_address = MYNEWT_VAL(ANCHOR_ADDRESS);
        uwb_set_uid(udev, udev->my_short_address);
    }
#if MYNEWT_VAL(RNG_VERBOSE) > 1
    udev->config.rxdiag_enable = 1;
#else
    udev->config.rxdiag_enable = 0;
#endif
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}
