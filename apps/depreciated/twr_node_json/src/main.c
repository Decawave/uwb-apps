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
#include <math.h>
#include "sysinit/sysinit.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_bsp.h"


#include <dw1000/dw1000_dev.h>
#include <dw1000/dw1000_hal.h>
#include <dw1000/dw1000_phy.h>
#include <dw1000/dw1000_mac.h>
#include <dw1000/dw1000_ftypes.h>

#if MYNEWT_VAL(DW1000_LWIP)
#include <dw1000/dw1000_lwip.h>
#endif
#if MYNEWT_VAL(RNG_ENABLED)
#include <rng/rng.h>
#endif

#include <json_encode.h>

/* The timer callout */
static struct os_callout blinky_callout;

#define SAMPLE_FREQ 10.0
static void timer_ev_cb(struct os_event *ev) {
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);

    hal_gpio_toggle(LED_BLINK_PIN);
    os_callout_reset(&blinky_callout, OS_TICKS_PER_SEC/SAMPLE_FREQ);
     
    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *)ev->ev_arg;
    dw1000_rng_instance_t * rng = inst->rng; 

    dw1000_rng_request(inst, 0x4321, DWT_DS_TWR);

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
            json_rng_encode(frame, 1);   
            printf("{\"utime\": %lu,\"tof\": %lu,\"range\": %lu}\n",
                    os_cputime_ticks_to_usecs(os_cputime_get32()),
                    time_of_flight,
                    *(uint32_t *)&range
            );
    }
   if (frame->code == DWT_DS_TWR_FINAL || frame->code == DWT_DS_TWR_EXT_FINAL) {
            json_rng_encode((frame-1), 2);
            uint32_t time_of_flight = (uint32_t) dw1000_rng_twr_to_tof(rng);        
            float range = dw1000_rng_tof_to_meters(dw1000_rng_twr_to_tof(rng));
            cir_t cir; 
            cir.fp_idx = dw1000_read_reg(inst, RX_TIME_ID, RX_TIME_FP_INDEX_OFFSET, sizeof(uint16_t));
            cir.fp_idx = roundf(((float) (cir.fp_idx >> 6) + 0.5f)) - 2;
            dw1000_read_accdata(inst, (uint8_t *)&cir,  cir.fp_idx * sizeof(cir_complex_t), CIR_SIZE * sizeof(cir_complex_t) + 1);
            json_cir_encode(&cir, "cir", CIR_SIZE);

            if(inst->config.rxdiag_enable)
            json_rxdiag_encode(&inst->rxdiag, "rxdiag");
            printf("{\"utime\": %lu,\"tof\": %lu,\"range\": %lu}\n",
                    os_cputime_ticks_to_usecs(os_cputime_get32()), 
                    time_of_flight, 
                     *(uint32_t *)&range
                );
            frame->code = DWT_DS_TWR_END;
    }
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
    init_timer(inst);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}

