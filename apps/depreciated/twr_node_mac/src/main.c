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

#if MYNEWT_VAL(DW1000_CCP_ENABLED)
#include <dw1000/dw1000_ccp.h>
#endif
#if MYNEWT_VAL(DW1000_LWIP)
#include <dw1000/dw1000_lwip.h>
#endif
#if MYNEWT_VAL(DW1000_PAN)
#include <dw1000/dw1000_pan.h>
#endif


static dw1000_rng_config_t rng_config = {
    .tx_holdoff_delay = 0x0600,         // Send Time delay in usec.
    .rx_timeout_period = 0x0800         // Receive response timeout in usec
};

#if MYNEWT_VAL(DW1000_PAN)
static dw1000_pan_config_t pan_config = {
    .tx_holdoff_delay = 0x0C00,         // Send Time delay in usec.
    .rx_timeout_period = 0x8000         // Receive response timeout in usec.
};
#endif

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
static struct os_callout blinky_callout;

/*
 * Event callback function for timer events. It toggles the led pin.
*/

#define SAMPLE_FREQ 50.0

static void timer_ev_cb(struct os_event *ev) {
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);

    hal_gpio_toggle(LED_BLINK_PIN);
    
    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *)ev->ev_arg;
    dw1000_rng_instance_t * rng = inst->rng; 

    dw1000_rng_request(inst, 0xDEC1, DWT_DS_TWR);

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
   
    if (frame->code == DWT_SS_TWR_FINAL) {
            uint32_t time_of_flight = (uint32_t) dw1000_rng_twr_to_tof(rng);
            float range = dw1000_rng_tof_to_meters(dw1000_rng_twr_to_tof(rng));
            float rssi = dw1000_get_rssi(inst);
            print_frame("trw=", frame);
            printf("{\"utime\": %lu,\"tof\": %lu,\"range\": %lu,\"res_req\": %lX,"
                   " \"rec_tra\": %lX, \"rssi\": %d}\n",
                    os_cputime_ticks_to_usecs(os_cputime_get32()),
                    time_of_flight,
                    (uint32_t)(range * 1000),
                   (frame->response_timestamp - frame->request_timestamp),
                   (frame->transmission_timestamp - frame->reception_timestamp),
                   (int)(rssi)
            );
    }

    if (frame->code == DWT_DS_TWR_FINAL || frame->code == DWT_DS_TWR_EXT_FINAL) {
            uint32_t time_of_flight = (uint32_t) dw1000_rng_twr_to_tof(rng);
            float range = dw1000_rng_tof_to_meters(dw1000_rng_twr_to_tof(rng));
            float rssi = dw1000_get_rssi(inst);
            print_frame("1st=", previous_frame);
            print_frame("2nd=", frame);
            printf("{\"utime\": %lu,\"tof\": %lu,\"range\": %lu,\"res_req\": %lX,"
                   " \"rec_tra\": %lX, \"rssi\": %d}\n",
                    os_cputime_ticks_to_usecs(os_cputime_get32()), 
                    time_of_flight, 
                    (uint32_t)(range * 1000), 
                   (frame->response_timestamp - frame->request_timestamp),
                   (frame->transmission_timestamp - frame->reception_timestamp),
                    (int)(rssi)
                );
            frame->code = DWT_DS_TWR_END;
    }
    os_callout_reset(&blinky_callout, OS_TICKS_PER_SEC/SAMPLE_FREQ);
}

/*
* Initialize the callout for a timer event.
*/
static void init_timer(dw1000_dev_instance_t * inst) {
    os_callout_init(&blinky_callout, os_eventq_dflt_get(), timer_ev_cb, inst);
    os_callout_reset(&blinky_callout, OS_TICKS_PER_SEC);
}

int main(int argc, char **argv){
    int rc;
    
    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);
    
    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);

    inst->PANID = 0xDECA;
    inst->my_short_address = MYNEWT_VAL(DW_DEVICE_ID_0);
    
#if MYNEWT_VAL(DW1000_MAC_FILTERING)
    dw1000_set_address16(inst, inst->my_short_address);
#endif 
    dw1000_set_panid(inst,inst->PANID);
    inst->config.framefilter_enabled = 0;
    dw1000_mac_init(inst, NULL);
    dw1000_rng_init(inst, &rng_config, sizeof(twr)/sizeof(twr_frame_t));
    dw1000_rng_set_frames(inst, twr, sizeof(twr)/sizeof(twr_frame_t));
#if MYNEWT_VAL(DW1000_CCP_ENABLED)
    dw1000_ccp_init(inst, 2, MYNEWT_VAL(UUID_CCP_MASTER));
#endif
#if MYNEWT_VAL(DW1000_PAN)
    dw1000_pan_init(inst, &pan_config);   
    dw1000_pan_start(inst, DWT_NONBLOCKING); 
        while(inst->pan->status.valid != true){
        os_eventq_run(os_eventq_dflt_get());
        os_cputime_delay_usecs(5000);
    } 
    inst->config.framefilter_enabled = 1;
    dw1000_set_address16(inst, inst->my_short_address);
    dw1000_mac_init(inst, NULL);
#endif
    printf("device_id = 0x%lX\n",inst->device_id);
    printf("PANID = 0x%X\n",inst->PANID);
    printf("DeviceID = 0x%X\n",inst->my_short_address);
    printf("partID = 0x%lX\n",inst->partID);
    printf("lotID = 0x%lX\n",inst->lotID);
    printf("xtal_trim = 0x%X\n",inst->xtal_trim);
  
    init_timer(inst);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}

