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
#include <json_encode.h>

static dwt_config_t mac_config = {
    .chan = 5,                          // Channel number. 
    .prf = DWT_PRF_64M,                 // Pulse repetition frequency. 
    .txPreambLength = DWT_PLEN_512,     // Preamble length. Used in TX only. 
    .rxPAC = DWT_PAC8,                  // Preamble acquisition chunk size. Used in RX only. 
    .txCode = 8,                        // TX preamble code. Used in TX only. 
    .rxCode = 9,                        // RX preamble code. Used in RX only. 
    .nsSFD = 0,                         // 0 to use standard SFD, 1 to use non-standard SFD. 
    .dataRate = DWT_BR_6M8,             // Data rate. 
    .phrMode = DWT_PHRMODE_STD,         // PHY header mode. 
    .sfdTO = (512 + 1 + 8 - 8)        // SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. 
};

static dw1000_phy_txrf_config_t txrf_config = { 
        .PGdly = TC_PGDELAY_CH5,
        //.power = 0x25456585
        .BOOSTNORM = dw1000_power_value(DW1000_txrf_config_0db, 3),
        .BOOSTP500 = dw1000_power_value(DW1000_txrf_config_0db, 3),    
        .BOOSTP250 = dw1000_power_value(DW1000_txrf_config_0db, 3),     
        .BOOSTP125 = dw1000_power_value(DW1000_txrf_config_0db, 3)
};

static dw1000_rng_config_t rng_config = {
    .tx_holdoff_delay = 0xA800,          // Send Time delay in usec.
    .rx_timeout_period = 0xF000         // Receive response timeout in usec.
};

static twr_frame_t twr[] = {
    [0].request = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID
    },
    [1].request = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID
    }
};


/* The timer callout */
static struct os_callout blinky_callout;
/*
 * Event callback function for timer events. It toggles the led pin.
*/
static void timer_ev_cb(struct os_event *ev) {
    assert(ev != NULL);

    hal_gpio_toggle(LED_BLINK_PIN);
    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);

    dw1000_rng_request(inst, 0x4321, DWT_DS_TWR);

    if (inst->status.start_tx_error || inst->status.rx_error || inst->status.request_timeout ||  inst->status.rx_timeout_error){
        inst->status.start_tx_error = inst->status.rx_error = inst->status.request_timeout = inst->status.rx_timeout_error = 0;
    }
        
    else if (inst->rng->twr[0].response.code == DWT_SS_TWR_FINAL) {
            twr_frame_t * twr  = &inst->rng->twr[0]; 
            json_rng_encode(inst->rng->twr, 1);   

            int32_t ToF = ((twr->response_timestamp - twr->request_timestamp) 
                -  (twr->response.transmission_timestamp - twr->response.reception_timestamp))/2;
            //float range = ToF * 299792458 * (1.0/499.2e6/128.0);
            //printf("ToF = %lX, range = %f\n", ToF, range);
            printf("ToF=%lX, res_req=%lX rec_tra=%lX\n", ToF, (twr->response_timestamp - twr->request_timestamp), (twr->response.transmission_timestamp - twr->response.reception_timestamp));
   
        } else if (inst->rng->nframes > 1){
                if (inst->rng->twr[1].response.code == DWT_DS_TWR_FINAL) {
                   
                    uint64_t T1R = (inst->rng->twr[0].response_timestamp - inst->rng->twr[0].request_timestamp); 
                    uint64_t T1r = (inst->rng->twr[0].response.transmission_timestamp  - inst->rng->twr[0].response.reception_timestamp); 
                    uint64_t T2R = (inst->rng->twr[1].response_timestamp - inst->rng->twr[1].request_timestamp); 
                    uint64_t T2r = (inst->rng->twr[1].response.transmission_timestamp  - inst->rng->twr[1].response.reception_timestamp); 
                    uint32_t Tp  =  (T1R - T1r + T2R - T2r);//  >> 2;
                    json_rng_encode(inst->rng->twr, inst->rng->nframes);

                    cir_t cir; 
                    dw1000_read_accdata(inst, (uint8_t * ) &cir, 600 * sizeof(cir_complex_t), CIR_SIZE * sizeof(cir_complex_t) + 1 );
                    cir.fp_idx = dw1000_read_reg(inst,  RX_TIME_ID,  RX_TIME_FP_INDEX_OFFSET, sizeof(uint16_t));
                    json_cir_encode(&cir, "cir", CIR_SIZE);

                   if(inst->config.rxdiag_enable)
                        json_rxdiag_encode(&inst->rxdiag, "rxdiag");

                    printf("{\"utime\": %ld,\"fp_idx\": %d,\"Tp\": %ld}\n", os_time_get(), cir.fp_idx, Tp);
                }
    }
    os_callout_reset(&blinky_callout, OS_TICKS_PER_SEC/32);
}

/*
* Initialize the callout for a timer event.
*/
static void init_timer(void) {
    os_callout_init(&blinky_callout, os_eventq_dflt_get(), timer_ev_cb, NULL);
    os_callout_reset(&blinky_callout, OS_TICKS_PER_SEC);
}


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
    inst->my_short_address = 0x1234;
    dw1000_set_panid(inst,inst->PANID);
    dw1000_mac_init(inst, &mac_config);
    dw1000_rng_init(inst, &rng_config);
    dw1000_rng_set_frames(inst, twr, sizeof(twr)/sizeof(twr_frame_t));

    init_timer();

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}

