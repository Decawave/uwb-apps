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

#if MYNEWT_VAL(DW1000_CLOCK_CALIBRATION)
#include <dw1000/dw1000_ccp.h>
#endif
#if MYNEWT_VAL(DW1000_LWIP)
#include <dw1000/dw1000_lwip.h>
#endif
#if MYNEWT_VAL(DW1000_PAN)
#include <dw1000/dw1000_pan.h>
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
    .tx_holdoff_delay = 0x1600,         // Send Time delay in usec.
    .rx_timeout_period = 0x1800         // Receive response timeout in usec
};

#if MYNEWT_VAL(DW1000_PAN)
static dw1000_pan_config_t pan_config = {
    .tx_holdoff_delay = 0x0C00,         // Send Time delay in usec.
    .rx_timeout_period = 0x8000         // Receive response timeout in usec.
};
#endif

static twr_frame_t twr_A[] = {
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

static twr_frame_t twr_B[] = {
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

static twr_frame_t * twr[] = {
    twr_A,
    twr_B
};

void print_frame(const char * name, twr_frame_t *twr ){
    printf("%s{\n\tfctrl:0x%04X,\n", name, twr->fctrl);
    printf("\tseq_num:0x%02X,\n", twr->seq_num);
    printf("\tPANID:0x%04X,\n", twr->PANID);
    printf("\tdst_address:0x%04X,\n", twr->dst_address);
    printf("\tsrc_address:0x%04X,\n", twr->src_address);
    printf("\tcode:0x%04X,\n", twr->code);
    printf("\treception_timestamp:0x%08lX,\n", twr->reception_timestamp);
    printf("\ttransmission_timestamp:0x%08lX,\n", twr->transmission_timestamp);
    printf("\trequest_timestamp:0x%08lX,\n", twr->request_timestamp);
    printf("\tresponse_timestamp:0x%08lX\n}\n", twr->response_timestamp);
}


/* The timer callout */
static struct os_callout aoa_callout;

/*
 * Event callback function for timer events. It toggles the led pin.
*/


#define SAMPLE_FREQ 10.0
static void timer_ev_cb(struct os_event *ev) {
    float rssi;
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);

    hal_gpio_toggle(LED_BLINK_PIN);
    os_callout_reset(&aoa_callout, OS_TICKS_PER_SEC/SAMPLE_FREQ);
    
    dw1000_dev_instance_t ** _inst = (dw1000_dev_instance_t **)ev->ev_arg;
    for (uint8_t i=0; i< 2; i++){
        
        dw1000_dev_instance_t * inst = _inst[i];
        dw1000_rng_instance_t * rng = inst->rng; 

        assert(inst->rng->nframes > 0);

        twr_frame_t * previous_frame = rng->frames[(rng->idx-1)%rng->nframes];
        twr_frame_t * frame = rng->frames[(rng->idx)%rng->nframes];

        if (inst->status.start_rx_error)
            printf("{\"utime\": %lu,\"timer_ev_cb\": \"start_rx_error\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()));
        if (inst->status.start_tx_error)
            printf("{\"utime\": %lu,\"timer_ev_cb\":\"start_tx_error\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()));
        if (inst->status.rx_error)
            printf("{\"utime\": %lu,\"timer_ev_cb\":\"rx_error\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()));
        if (inst->status.rx_timeout_error)
            printf("{\"utime\": %lu,\"timer_ev_cb\":\"rx_timeout_error\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()));
    
        if (inst->status.start_tx_error || inst->status.start_rx_error || inst->status.rx_error 
            ||  inst->status.rx_timeout_error){
            dw1000_set_rx_timeout(inst, 0);
            dw1000_start_rx(inst); 
        }

        else if (frame->code == DWT_DS_TWR_FINAL || frame->code == DWT_DS_TWR_EXT_FINAL) {
            uint32_t time_of_flight = (uint32_t) dw1000_rng_twr_to_tof(rng);
            float range = dw1000_rng_tof_to_meters(dw1000_rng_twr_to_tof(rng));
            dw1000_get_rssi(inst, &rssi);
            print_frame("1st=", previous_frame);
            print_frame("2nd=", frame);
            frame->code = DWT_DS_TWR_END;
                printf("{\"utime\": %lu,\"tof\": %lu,\"range\": %lu,\"res_req\": %lX,"
                    " \"rec_tra\": %lX, \"rssi\": %d, \"Node\": %c}\n",
                os_cputime_ticks_to_usecs(os_cputime_get32()),
                time_of_flight, 
                (uint32_t)(range * 1000), 
                (frame->response_timestamp - frame->request_timestamp),
                (frame->transmission_timestamp - frame->reception_timestamp),
                (int)(rssi),
                'A' + i
            );
            dw1000_set_rx_timeout(inst, 0);
            dw1000_start_rx(inst); 
        }
    }
}


/*
* Initialize the callout for a timer event.
*/
static void init_timer(dw1000_dev_instance_t * inst[]) {
    os_callout_init(&aoa_callout, os_eventq_dflt_get(), timer_ev_cb, inst);
    os_callout_reset(&aoa_callout, OS_TICKS_PER_SEC);
}


static void aoa_clk_sync(dw1000_dev_instance_t * inst[], uint8_t n){
    for (uint8_t i = 0; i < n; i++ )
        dw1000_phy_external_sync(inst[i],33, true);

    hal_gpio_init_out(MYNEWT_VAL(DW1000_AOA_SYNC_EN), 1);
    hal_gpio_init_out(MYNEWT_VAL(DW1000_AOA_SYNC_CLR), 1);

    hal_gpio_init_out(MYNEWT_VAL(DW1000_AOA_SYNC), 1);
    os_cputime_delay_usecs(5000);
    hal_gpio_write(MYNEWT_VAL(DW1000_AOA_SYNC), 0);

    hal_gpio_write(MYNEWT_VAL(DW1000_AOA_SYNC_CLR), 0);
    hal_gpio_write(MYNEWT_VAL(DW1000_AOA_SYNC_EN), 0);
    
    for (uint8_t i = 0; i < n; i++ )
        dw1000_phy_external_sync(inst[i],0, false);
}

int main(int argc, char **argv){
    int rc;
    
    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);

    dw1000_dev_instance_t * inst[2] = {0,0};

    for (uint8_t i=0; i < 2; i++){
        inst[i] = hal_dw1000_inst(i);
        dw1000_softreset(inst[i]);
        dw1000_phy_init(inst[i], &txrf_config);   
        inst[i]->PANID = 0xDECA;
        inst[i]->my_short_address = MYNEWT_VAL(DEVICE_ID);
        inst[i]->my_long_address = ((uint64_t) inst[i]->device_id << 32) + inst[i]->partID;

        dw1000_set_panid(inst[i],inst[i]->PANID);
        dw1000_mac_init(inst[i], &mac_config);
        dw1000_rng_init(inst[i], &rng_config, 2);
        dw1000_rng_set_frames(inst[i], twr[i], 2);
#if MYNEWT_VAL(DW1000_CLOCK_CALIBRATION)
        dw1000_ccp_init(inst[i], 2, MYNEWT_VAL(UUID_CCP_MASTER));
#endif
#if MYNEWT_VAL(DW1000_PAN)
        dw1000_pan_init(inst[i], &pan_config);   
        dw1000_pan_start(ins[i], DWT_NONBLOCKING);  
#endif
        printf("device_id = 0x%lX\n",inst[i]->device_id);
        printf("PANID = 0x%X\n",inst[i]->PANID);
        printf("DeviceID = 0x%X\n",inst[i]->my_short_address);
        printf("partID = 0x%lX\n",inst[i]->partID);
        printf("lotID = 0x%lX\n",inst[i]->lotID);
        printf("xtal_trim = 0x%X\n",inst[i]->xtal_trim);
    }

    aoa_clk_sync(inst, 2);

    init_timer(inst);

    for (uint8_t i=0; i < 2; i++){
        dw1000_set_rx_timeout(inst[i], 0);
        dw1000_start_rx(inst[i]);
    }
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}

