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

#include <dw1000/dw1000_dev.h>
#include <dw1000/dw1000_hal.h>
#include <dw1000/dw1000_phy.h>
#include <dw1000/dw1000_mac.h>
#include <dw1000/dw1000_rng.h>
#include <dw1000/dw1000_lwip.h>
#include <dw1000/dw1000_ftypes.h>
#include <dw1000/dw1000_provision.h>

#define NUM_FRAMES 2
#define NUM_NODES 32

static dwt_config_t mac_config = {
    .chan = 5,                          // Channel number. 
    .prf = DWT_PRF_64M,                 // Pulse repetition frequency. 
    .txPreambLength = DWT_PLEN_256,     // Preamble length. Used in TX only. 
    .rxPAC = DWT_PAC8,                 // Preamble acquisition chunk size. Used in RX only. 
    .txCode = 9,                        // TX preamble code. Used in TX only. 
    .rxCode = 9,                        // RX preamble code. Used in RX only. 
    .nsSFD = 0,                         // 0 to use standard SFD, 1 to use non-standard SFD. 
    .dataRate = DWT_BR_6M8,             // Data rate. 
    .phrMode = DWT_PHRMODE_STD,         // PHY header mode. 
    .sfdTO = (256 + 1 + 8 - 8)          // SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. 
};

static dw1000_phy_txrf_config_t txrf_config = {
        .PGdly = TC_PGDELAY_CH5,
        //.power = 0x25456585
        .BOOSTNORM = dw1000_power_value(DW1000_txrf_config_9db, 5),
        .BOOSTP500 = dw1000_power_value(DW1000_txrf_config_9db, 5),
        .BOOSTP250 = dw1000_power_value(DW1000_txrf_config_9db, 5),
        .BOOSTP125 = dw1000_power_value(DW1000_txrf_config_9db, 5)
};
static twr_frame_t twr[] = {
    [0] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                 // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID
    },
    [1] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                 // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID,
    }
};
static dw1000_rng_config_t rng_config = {
    .tx_holdoff_delay = 0x0800,         // Send Time delay in usec.
    .rx_timeout_period = 0xC000         // Receive response timeout in usec.
};

static struct os_callout blinky_callout;

static void
timer_ev_cb(struct os_event *ev){
    assert(ev != NULL);
    dw1000_dev_instance_t* inst = (dw1000_dev_instance_t*)ev->ev_arg;

    if(inst->status.rx_timeout_error || inst->status.start_rx_error || inst->status.start_tx_error){
        inst->status.rx_timeout_error = inst->status.start_rx_error  = inst->status.start_tx_error = 0;
        dw1000_set_rx_timeout(inst, 0);
        dw1000_start_rx(inst);
    }
    os_callout_reset(&blinky_callout, OS_TICKS_PER_SEC/64);
}

static void init_timer(dw1000_dev_instance_t* inst) {
    /*
     * Initialize the callout for a timer event.
     */
    os_callout_init(&blinky_callout, os_eventq_dflt_get(), timer_ev_cb, inst);
    os_callout_reset(&blinky_callout, OS_TICKS_PER_SEC);
}

int main(int argc, char **argv){
    int rc;
    dw1000_provision_config_t config;

    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);

    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
    dw1000_softreset(inst);
    dw1000_phy_init(inst, &txrf_config);

    inst->PANID =  MYNEWT_VAL(DEVICE_PAN_ID);
    inst->my_short_address =  MYNEWT_VAL(DEVICE_ID);
    inst->slot_id = MYNEWT_VAL(SLOT_ID);
    inst->dev_type = MYNEWT_VAL(DEVICE_TYPE);

    config.tx_holdoff_delay = rng_config.tx_holdoff_delay;
    config.rx_timeout_period = rng_config.rx_timeout_period;
    config.period = MYNEWT_VAL(PROVISION_PERIOD)*1e-3;
    config.postprocess = false;
    config.max_node_count = NUM_NODES;

    dw1000_set_panid(inst,inst->PANID);
    dw1000_mac_init(inst, &mac_config);
    dw1000_rng_init(inst, &rng_config, NUM_FRAMES);
    dw1000_rng_set_frames(inst, twr, NUM_FRAMES);

    printf("\nNODE:%d_______Address:0x%04X\n",inst->slot_id,inst->my_short_address);

    dw1000_provision_init(inst,config);
    dw1000_set_rx_timeout(inst, 0);
    dw1000_start_rx(inst);
    init_timer(inst);
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}
