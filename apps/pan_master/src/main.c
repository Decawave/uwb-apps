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
#include <dw1000/dw1000_ftypes.h>
#include <dw1000/dw1000_pan.h>

#if MYNEWT_VAL(DW1000_CLOCK_CALIBRATION)
#include <dw1000/dw1000_ccp.h>
#endif

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
    .sfdTO = (256 + 1 + 8 - 8)         // SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. 
};

static dw1000_phy_txrf_config_t txrf_config = { 
        .PGdly = TC_PGDELAY_CH5,
        //.power = 0x25456585
        .BOOSTNORM = dw1000_power_value(DW1000_txrf_config_9db, 5),
        .BOOSTP500 = dw1000_power_value(DW1000_txrf_config_9db, 5),
        .BOOSTP250 = dw1000_power_value(DW1000_txrf_config_9db, 5),
        .BOOSTP125 = dw1000_power_value(DW1000_txrf_config_9db, 5)
};

static dw1000_rng_config_t rng_config = {
    .tx_holdoff_delay = 0x0C00,         // Send Time delay in usec.
    .rx_timeout_period = 0x0800         // Receive response timeout in usec.
};

static dw1000_pan_config_t pan_config = {
    .tx_holdoff_delay = 0x0C00,         // Send Time delay in usec.
    .rx_timeout_period = 0x2000         // Receive response timeout in usec.
};

static twr_frame_t twr[] = {
    [0] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID
    },
    [1] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID,
    }
};

#if 1
#define PAN_SIZE 16
static uint16_t g_device_idx = 0;
typedef struct _pan_db {
    uint64_t UUID;
    uint32_t short_address;
    uint8_t slot_id;
}pan_db_t;

pan_db_t g_pan_db[PAN_SIZE];

static void 
pan_master(struct os_event * ev){
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);
    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *)ev->ev_arg;
    dw1000_pan_instance_t * pan = inst->pan; 
    pan_frame_t * frame = pan->frames[(pan->idx)%pan->nframes]; 

    //PAN Request frame
    printf("{\"utime\":%lu,\"UUID\":\"%llX\",\"seq_num\": %d}\n", 
        os_cputime_ticks_to_usecs(os_cputime_get32()),
        frame->long_address,
        frame->seq_num
    );

    uint8_t i = 0;
    for (i = 0; i < g_device_idx; i++){
        if (g_pan_db[i%PAN_SIZE].UUID == frame->long_address) {
             // Retransmit  
            frame->pan_id = inst->PANID; 
            frame->short_address = g_pan_db[i].short_address; 
            frame->slot_id = g_pan_db[i].slot_id;
            break;
        }
    }
    if (i == g_device_idx){
        // Assign new IDs
        g_pan_db[g_device_idx%PAN_SIZE].UUID = frame->long_address;
        g_pan_db[g_device_idx%PAN_SIZE].slot_id = frame->slot_id = (g_device_idx + 1)%PAN_SIZE;
        g_pan_db[g_device_idx%PAN_SIZE].short_address = frame->short_address = 0xDEC0 + g_device_idx + 1;
        frame->pan_id = inst->PANID;
        frame->seq_num++;

        g_device_idx++;
    }
    if(g_device_idx > PAN_SIZE)
    {
        printf("{\"utime\":%lu,\"Warning\": \"PANIDs over subscribed\",{\"g_device_idx\":%d}\n", 
            os_cputime_ticks_to_usecs(os_cputime_get32()),
            g_device_idx
        );  
    }

    printf("{\"utime\":%lu,\"UUID\":\"%llX\",\"ID\":\"%X\",\"PANID\":\"%X\",\"SLOTID\":%d}\n", 
        os_cputime_ticks_to_usecs(os_cputime_get32()),
        frame->long_address,
        frame->short_address,
        frame->pan_id,
        frame->slot_id
    );

    dw1000_write_tx(inst, frame->array, 0, sizeof(pan_frame_resp_t));
    dw1000_write_tx_fctrl(inst, sizeof(pan_frame_resp_t), 0, true); 
    dw1000_set_wait4resp(inst, false);    
    pan->status.start_tx_error = dw1000_start_tx(inst).start_tx_error;
}
#endif

int main(int argc, char **argv){
    int rc;
    
    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);
    
    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
    dw1000_softreset(inst);
    dw1000_phy_init(inst, &txrf_config);   

    inst->PANID = 0xDECA;
    inst->my_short_address = MYNEWT_VAL(DEVICE_ID); 
    inst->my_long_address = ((uint64_t) inst->device_id << 32) + inst->partID;
    
    dw1000_set_panid(inst,inst->PANID);
    dw1000_mac_init(inst, &mac_config);
    dw1000_rng_init(inst, &rng_config, sizeof(twr)/sizeof(twr_frame_t));
    dw1000_rng_set_frames(inst, twr, sizeof(twr)/sizeof(twr_frame_t));  
#if MYNEWT_VAL(DW1000_CLOCK_CALIBRATION)
    dw1000_ccp_init(inst, 2, inst->my_long_address);
    dw1000_ccp_start(inst);
#endif
    dw1000_pan_init(inst, &pan_config);  
    dw1000_pan_set_postprocess(inst, pan_master);
    dw1000_set_rx_timeout(inst, 0);
    dw1000_start_rx(inst);  

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
  
        if (inst->status.start_rx_error)
            printf("{\"utime\": %lu,\"timer_ev_cb\": \"start_rx_error\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()));
        if (inst->status.start_tx_error)
            printf("{\"utime\": %lu,\"timer_ev_cb\":\"start_tx_error\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()));
        if (inst->status.rx_error)
            printf("{\"utime\": %lu,\"timer_ev_cb\":\"rx_error\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()));
        if (inst->status.rx_timeout_error)
            printf("{\"utime\": %lu,\"timer_ev_cb\":\"rx_timeout_error\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()));

        if (inst->status.start_tx_error || inst->status.start_rx_error ||
            inst->status.rx_error || inst->status.rx_timeout_error) {
            dw1000_set_rx_timeout(inst, 0);
            dw1000_start_rx(inst); 
        }
    }
    assert(0);
    return rc;
}

