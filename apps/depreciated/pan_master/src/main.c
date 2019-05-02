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
#include <dw1000/dw1000_ftypes.h>

#if MYNEWT_VAL(RNG_ENABLED)
#include <rng/rng.h>
#endif
#if MYNEWT_VAL(PAN_ENABLED)
#include <pan/pan.h>
#endif
#if MYNEWT_VAL(CCP_ENABLED)
#include <ccp/ccp.h>
#endif

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
    if(g_device_idx > PAN_SIZE){
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

    dw1000_write_tx(inst, frame->array, 0, sizeof(pan_frame_t));
    dw1000_write_tx_fctrl(inst, sizeof(pan_frame_t), 0);
    dw1000_set_wait4resp(inst, true);    
    dw1000_set_rx_timeout(inst, 0);
    pan->status.start_tx_error = dw1000_start_tx(inst).start_tx_error;
}

int main(int argc, char **argv){
    int rc;
    
    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);
    
    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);

    dw1000_pan_set_postprocess(inst, pan_master);
    dw1000_set_rx_timeout(inst, 0);
    dw1000_start_rx(inst);  

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    assert(0);
    return rc;
}
