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
#include "sysinit/sysinit.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_bsp.h"
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif

#include "bleprph.h"

#include "sensor/sensor.h"
#include "sensor/accel.h"
#include "sensor/gyro.h"
#include "sensor/mag.h"
#include "sensor/pressure.h"
#include "console/console.h"
#include <config/config.h>
#include "uwbcfg/uwbcfg.h"

#include "dw1000/dw1000_ftypes.h"
#include "dw1000/dw1000_dev.h"
#include "dw1000/dw1000_hal.h"
#include "dw1000/dw1000_phy.h"
#include "dw1000/dw1000_mac.h"

/* The timer callout */
static struct os_callout tdoa_callout;

static bool config_changed = false;


int
uwb_sleep(void)
{
#if MYNEWT_VAL(DW1000_DEVICE_0)
    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);

    /* Enter sleep */
    dw1000_phy_forcetrxoff(inst);
    dw1000_phy_rx_reset(inst);
    dw1000_dev_configure_sleep(inst);
    /* Clear all status flags that could prevent entering sleep */
    dw1000_write_reg(inst, SYS_STATUS_ID,3,0xFFFF,sizeof(uint16_t));
    dw1000_write_reg(inst, SYS_STATUS_ID,0,0xFFFFFFFF,sizeof(uint32_t));
    dw1000_dev_configure_sleep(inst);
    /* Enter sleep */
    dw1000_dev_enter_sleep(inst);
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
void tdoa_timer_ev_cb(struct os_event *ev) {
    assert(ev != NULL);

    hal_gpio_write(LED_BLINK_PIN, 1);

    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
    dw1000_dev_wakeup(inst);

    if (config_changed) {
        dw1000_mac_config(inst, NULL);
        dw1000_phy_config_txrf(inst, &inst->config.txrf);
        config_changed = false;
    }

    dw1000_dev_configure_sleep(inst);
    dw1000_dev_enter_sleep_after_tx(inst, 1);

    dw1000_write_tx(inst, tdoa_blink_frame.array, 0, sizeof(ieee_blink_frame_t));
    dw1000_write_tx_fctrl(inst, sizeof(ieee_blink_frame_t), 0);

    if (dw1000_start_tx(inst).start_tx_error){
        printf("err\n");
    }
    hal_gpio_write(LED_BLINK_PIN, 0);
    
    tdoa_blink_frame.seq_num++;
    os_callout_reset(&tdoa_callout, OS_TICKS_PER_SEC/MYNEWT_VAL(TDOA_TAG_BLINK_RATE));
}


static void init_timer(void) {
    /*
     * Initialize the callout for a timer event.
     */
    os_callout_init(&tdoa_callout, os_eventq_dflt_get(), tdoa_timer_ev_cb, NULL);
    os_callout_reset(&tdoa_callout, OS_TICKS_PER_SEC);
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
    conf_load();
    
    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
    inst->config.dblbuffon_enabled = 0;
    inst->config.framefilter_enabled = 0;
    inst->config.bias_correction_enable = 0;
    inst->config.LDE_enable = 1;
    inst->config.LDO_enable = 0;
    inst->config.sleep_enable = 1;
    inst->config.wakeup_rx_enable = 0;
    inst->config.rxauto_enable = 0;

    /* Use the lot and device id as the defaul euid/long address */
    inst->my_short_address = inst->partID&0xffff;
    inst->my_long_address = ((uint64_t) inst->lotID << 33) + inst->partID;

    dw1000_set_eui(inst,inst->my_long_address);
    dw1000_set_panid(inst,inst->PANID);
    dw1000_set_rx_timeout(inst, 0);
    uwb_sleep();
    
    printf("{\"device_id\"=\"%lX\"",inst->device_id);
    printf(",\"PANID=\"%X\"",inst->PANID);
    printf(",\"addr\"=\"%X\"",inst->my_short_address);
    printf(",\"partID\"=\"%lX\"",inst->partID);
    printf(",\"lotID\"=\"%lX\"}\n",inst->lotID);

    tdoa_blink_frame.long_address = inst->my_long_address;
#if MYNEWT_VAL(BLE_ENABLED)
    ble_init(inst->my_long_address);
#endif    

    init_timer();
    hal_gpio_init_out(LED_BLINK_PIN, 0);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());   
    }

    assert(0);
    return rc;
}

