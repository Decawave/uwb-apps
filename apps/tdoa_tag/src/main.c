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
#include "sysinit/sysinit.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_bsp.h"
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif

#if MYNEWT_VAL(BLE_ENABLED)
#include "bleprph/bleprph.h"
#endif

#include "sensor/sensor.h"
#include "sensor/accel.h"
#include "sensor/gyro.h"
#include "sensor/mag.h"
#include "sensor/pressure.h"
#include "console/console.h"
#include <config/config.h>
#include "uwbcfg/uwbcfg.h"

#include <uwb/uwb.h>
#include <uwb/uwb_ftypes.h>

/* The timer callout */
static struct dpl_callout tdoa_callout;
static int16_t g_blink_rate = 1;
static bool config_changed = false;

/*
 * Config
 */
static char *uwb_conf_get(int argc, char **argv, char *val, int val_len_max);
static int uwb_conf_set(int argc, char **argv, char *val);
static int uwb_conf_commit(void);
static int uwb_conf_export(void (*export_func)(char *name, char *val),
  enum conf_export_tgt tgt);

static struct uwb_config_s {
    char blink_rate[8];
} uwb_config = { .blink_rate = {MYNEWT_VAL(TDOA_TAG_BLINK_RATE)} };

static struct conf_handler uwb_conf_cbs = {
    .ch_name = "main",
    .ch_get = uwb_conf_get,
    .ch_set = uwb_conf_set,
    .ch_commit = uwb_conf_commit,
    .ch_export = uwb_conf_export,
};

static char *
uwb_conf_get(int argc, char **argv, char *val, int val_len_max)
{
    if (argc == 1) {
        if (!strcmp(argv[0], "blink_rate")) {
            return uwb_config.blink_rate;
        }
    }
    return NULL;
}

static int
uwb_conf_set(int argc, char **argv, char *val)
{
    if (argc == 1) {
        if (!strcmp(argv[0], "blink_rate")) {
            return CONF_VALUE_SET(val, CONF_STRING, uwb_config.blink_rate);
        }
    }
    return DPL_ENOENT;
}

static int
uwb_conf_commit(void)
{
    int16_t old_blink_rate = g_blink_rate;
    conf_value_from_str(uwb_config.blink_rate, CONF_INT16,
                        (void*)&g_blink_rate, 0);

    /* Start blinking if we weren't before */
    if (g_blink_rate > 0 && old_blink_rate==0) {
        dpl_callout_reset(&tdoa_callout, DPL_TICKS_PER_SEC/g_blink_rate);
    }
    return 0;
}

static int
uwb_conf_export(void (*export_func)(char *name, char *val),
  enum conf_export_tgt tgt)
{
    export_func("main/blink_rate", uwb_config.blink_rate);
    return 0;
}


int
uwb_sleep(void)
{
#if MYNEWT_VAL(UWB_DEVICE_0)
    struct uwb_dev *udev = uwb_dev_idx_lookup(0);

    /* Enter sleep */
    uwb_phy_forcetrxoff(udev);
    uwb_phy_rx_reset(udev);
    uwb_sleep_config(udev);
    /* Clear all status flags that could prevent entering sleep */
    //dw1000_write_reg(inst, SYS_STATUS_ID,3,0xFFFF,sizeof(uint16_t));
    //dw1000_write_reg(inst, SYS_STATUS_ID,0,0xFFFFFFFF,sizeof(uint32_t));
    uwb_sleep_config(udev);
    /* Enter sleep */
    uwb_enter_sleep(udev);
#endif
    return 0;
}

/*
 * Event callback function for timer events.
*/
static ieee_blink_frame_t tdoa_blink_frame = {
    .fctrl = 0xC5,  /* frame type (0xC5 for a blink) using 64-bit addressing */
    .seq_num = 0,   /* sequence number, incremented for each new frame. */
    .long_address = 0,  /* device ID */
};

static
void tdoa_timer_ev_cb(struct dpl_event *ev) {
    assert(ev != NULL);

    hal_gpio_write(LED_BLINK_PIN, 1);

    struct uwb_dev *udev = uwb_dev_idx_lookup(0);
    if (udev->status.sleeping) {
        uwb_wakeup(udev);
    }

    if (config_changed) {
        uwb_mac_config(udev, NULL);
        uwb_txrf_config(udev, &udev->config.txrf);
        config_changed = false;
    }

    uwb_sleep_config(udev);
    uwb_enter_sleep_after_tx(udev, g_blink_rate < 10);

    uwb_write_tx_fctrl(udev, sizeof(ieee_blink_frame_t), 0);

    if (uwb_start_tx(udev).start_tx_error){
        printf("start tx err\n");
    } else {
        uwb_write_tx(udev, tdoa_blink_frame.array, 0, sizeof(ieee_blink_frame_t));
    }
    hal_gpio_write(LED_BLINK_PIN, 0);

    tdoa_blink_frame.seq_num++;
    if (g_blink_rate) {
        dpl_callout_reset(&tdoa_callout, DPL_TICKS_PER_SEC/g_blink_rate);
    }
}


static void init_timer(void) {
    /*
     * Initialize the callout for a timer event.
     */
    dpl_callout_init(&tdoa_callout, dpl_eventq_dflt_get(), tdoa_timer_ev_cb, NULL);
    if (g_blink_rate) {
        dpl_callout_reset(&tdoa_callout, DPL_TICKS_PER_SEC/g_blink_rate);
    }
}

int
uwb_config_updated()
{
    config_changed = true;
    return 0;
}

struct uwbcfg_cbs uwb_cb = {
    .uc_update = uwb_config_updated
};

int main(int argc, char **argv){
    int rc;

    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);

    uwbcfg_register(&uwb_cb);

    /* Register local config */
    rc = conf_register(&uwb_conf_cbs);
    assert(rc == 0);

    struct uwb_dev *udev = uwb_dev_idx_lookup(0);
    udev->config.dblbuffon_enabled = 0;
    udev->config.bias_correction_enable = 0;
    udev->config.LDE_enable = 1;
    udev->config.LDO_enable = 0;
    udev->config.sleep_enable = 1;
    udev->config.wakeup_rx_enable = 0;
    udev->config.rxauto_enable = 0;

    uwb_sleep();

    printf("{\"device_id\"=\"%lX\"",udev->device_id);
    printf(",\"panid=\"%X\"",udev->pan_id);
    printf(",\"addr\"=\"%X\"",udev->uid);
    printf(",\"part_id\"=\"%lX\"",(uint32_t)(udev->euid&0xffffffff));
    printf(",\"lot_id\"=\"%lX\"}\n",(uint32_t)(udev->euid>>32));

    tdoa_blink_frame.long_address = udev->my_long_address;
#if MYNEWT_VAL(BLE_ENABLED)
    ble_init(udev->my_long_address);
#endif

    init_timer();
    hal_gpio_init_out(LED_BLINK_PIN, 0);

    while (1) {
        dpl_eventq_run(dpl_eventq_dflt_get());
    }

    assert(0);
    return rc;
}
