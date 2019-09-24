/*
 * Copyright 2018, Decawave Limited, All Rights Reserved
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
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <os/os.h>
#include <hal/hal_spi.h>
#include <hal/hal_gpio.h>
#include "bsp/bsp.h"


#include <dw1000/dw1000_regs.h>
#include <dw1000/dw1000_dev.h>
#include <dw1000/dw1000_hal.h>
#include <dw1000/dw1000_mac.h>
#include <dw1000/dw1000_phy.h>
#include <dw1000/dw1000_ftypes.h>
#include <dw1000/dw1000_rng.h>

#if MYNEWT_VAL(DW1000_LWIP_P2P)
#include <dw1000_lwip_p2p.h>

static void lwip_p2p_postprocess(struct os_event * ev);
static void lwip_p2p_complete_cb(dw1000_dev_instance_t * inst);
static struct os_callout lwip_p2p_callout_timer;
static struct os_callout lwip_p2p_callout_postprocess;

static void rx_complete_cb(dw1000_dev_instance_t * inst);
static void tx_complete_cb(dw1000_dev_instance_t * inst);
static void rx_timeout_cb(dw1000_dev_instance_t * inst);
static void rx_error_cb(dw1000_dev_instance_t * inst);

static void
lwip_p2p_timer_ev_cb(struct os_event *ev) {
    
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);

    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *)ev->ev_arg;
    dw1000_lwip_p2p_instance_t * lwip_p2p = inst->lwip->lwip_p2p;
    
    uint32_t idx = lwip_p2p->idx++ % (lwip_p2p->nnodes-1);

    inst->lwip->dst_addr = lwip_p2p->payload_info[idx]->node_addr;
    dw1000_lwip_p2p_send(inst, idx);
    os_callout_reset(&lwip_p2p_callout_timer, OS_TICKS_PER_SEC/4);
    dw1000_lwip_start_rx(inst, 0xF000);
}

static void
lwip_p2p_timer_init(dw1000_dev_instance_t *inst) {

    os_callout_init(&lwip_p2p_callout_timer, os_eventq_dflt_get(), lwip_p2p_timer_ev_cb, (void *) inst);
    os_callout_reset(&lwip_p2p_callout_timer, OS_TICKS_PER_SEC/4);
    dw1000_lwip_p2p_instance_t * lwip_p2p = inst->lwip->lwip_p2p;
    lwip_p2p->status.timer_enabled = true;
}

static void 
lwip_p2p_complete_cb(dw1000_dev_instance_t *inst){

    dw1000_lwip_p2p_instance_t *lwip_p2p = inst->lwip->lwip_p2p;
    if(lwip_p2p->config.postprocess)
        os_eventq_put(os_eventq_dflt_get(), &lwip_p2p_callout_postprocess.c_ev);
}

static void 
lwip_p2p_postprocess(struct os_event * ev){

    assert(ev != NULL);
    assert(ev->ev_arg != NULL);
}

dw1000_lwip_p2p_instance_t *
dw1000_lwip_p2p_init(dw1000_dev_instance_t * inst, uint16_t nnodes,
                        dw1000_lwip_p2p_node_address_t node_addr[],  
                        dw1000_lwip_p2p_payload_info_t payload_info[]){
                        
    assert(inst);

    if (inst->lwip->lwip_p2p == NULL ) {
        inst->lwip->lwip_p2p = (dw1000_lwip_p2p_instance_t *)malloc( sizeof(dw1000_lwip_p2p_instance_t) + 
                                    (nnodes-1) * ( (sizeof(dw1000_lwip_p2p_payload_info_t *)) + 
                                        (sizeof(dw1000_lwip_p2p_node_address_t *))) );

        assert(inst->lwip->lwip_p2p);
        memset(inst->lwip->lwip_p2p, 0, sizeof(dw1000_lwip_p2p_instance_t));
        inst->lwip->lwip_p2p->status.selfmalloc = 1;
        inst->lwip->lwip_p2p->nnodes = nnodes;
    }
    else
        assert(inst->lwip->lwip_p2p->nnodes == nnodes);

    dw1000_lwip_p2p_set_frames(inst, nnodes, node_addr, payload_info);

    inst->lwip->lwip_p2p->parent = inst;
    inst->lwip->lwip_p2p->config = (dw1000_lwip_p2p_config_t){
        .postprocess = false,
    };

    dw1000_lwip_p2p_set_callbacks(inst, lwip_p2p_complete_cb, tx_complete_cb, rx_complete_cb, rx_timeout_cb, rx_error_cb);
    dw1000_lwip_p2p_set_postprocess(inst, &lwip_p2p_postprocess);
    inst->lwip->lwip_p2p->status.initialized = 1;
    return inst->lwip->lwip_p2p;
}

void 
dw1000_lwip_p2p_send(dw1000_dev_instance_t * inst, uint8_t idx){

    uint16_t payload_size = inst->lwip->lwip_p2p->payload_info[idx]->output_payload.payload_size;
    ip_addr_t *ipaddr =inst->lwip->lwip_p2p->payload_info[idx]->ip_addr;
    char * payload_p2p = (char *)inst->lwip->lwip_p2p->payload_info[idx]->output_payload.payload_ptr;

    dw1000_lwip_send(inst, payload_size, payload_p2p, ipaddr);
}

inline void
dw1000_lwip_p2p_set_frames(dw1000_dev_instance_t * inst,uint16_t nnodes,dw1000_lwip_p2p_node_address_t node_addr[], 
                             dw1000_lwip_p2p_payload_info_t payload_info[]){

    for(uint16_t i=0 ; i < ((nnodes) * (nnodes-1))/2 ; i++)
        inst->lwip->lwip_p2p->payload_info[i] = &payload_info[i];

    inst->lwip->lwip_p2p->nnodes = nnodes;
}

void
dw1000_lwip_p2p_free(dw1000_lwip_p2p_instance_t * inst){

    assert(inst);
    if (inst->status.selfmalloc)
        free(inst);
    else
        inst->status.initialized = 0;
}

void 
dw1000_lwip_p2p_set_callbacks(dw1000_dev_instance_t * inst, dw1000_dev_cb_t lwip_p2p_complete_cb, 
                                    dw1000_dev_cb_t tx_complete_cb, dw1000_dev_cb_t rx_complete_cb, 
                                    dw1000_dev_cb_t rx_timeout_cb, dw1000_dev_cb_t rx_error_cb){

    inst->lwip->ext_complete_cb      = lwip_p2p_complete_cb;
    inst->lwip->ext_tx_complete_cb   = tx_complete_cb;
    inst->lwip->ext_rx_complete_cb   = rx_complete_cb;
    inst->lwip->ext_rx_timeout_cb    = rx_timeout_cb;
    inst->lwip->ext_rx_error_cb      = rx_error_cb;
}

void
dw1000_lwip_p2p_set_postprocess(dw1000_dev_instance_t * inst, os_event_fn * lwip_p2p_postprocess){

    os_callout_init(&lwip_p2p_callout_postprocess, os_eventq_dflt_get(), lwip_p2p_postprocess, (void *) inst);
    dw1000_lwip_p2p_instance_t * lwip_p2p = inst->lwip->lwip_p2p;
    lwip_p2p->config.postprocess = true;
}

void
dw1000_lwip_p2p_start(dw1000_dev_instance_t * inst){

    dw1000_lwip_p2p_instance_t * lwip_p2p = inst->lwip->lwip_p2p;
    lwip_p2p->status.valid = false;
    lwip_p2p_timer_init(inst);
}

void
dw1000_lwip_p2p_stop(dw1000_dev_instance_t * inst){

    os_callout_stop(&lwip_p2p_callout_timer);
}

static void 
rx_complete_cb(dw1000_dev_instance_t * inst){


    uint32_t payload_size = inst->lwip->lwip_p2p->payload_info[0]->input_payload.payload_size;
    
    memcpy(inst->lwip->lwip_p2p->payload_info[0]->input_payload.payload_ptr, inst->lwip->payload_ptr, payload_size);
    inst->lwip->ext_complete_cb(inst);
}

static void 
tx_complete_cb(dw1000_dev_instance_t * inst){

    /* Nothing to do for now. Place holder for future
     * expansions
     */
}

static void 
rx_timeout_cb(dw1000_dev_instance_t * inst){

    inst->lwip->lwip_p2p->status.rx_timeout_error = 1;
    return;
}

void
rx_error_cb(dw1000_dev_instance_t * inst){

    inst->lwip->lwip_p2p->status.rx_error = 1;
    return;
}
#endif //MYNEWT_VAL(DW1000_LWIP_P2P)
