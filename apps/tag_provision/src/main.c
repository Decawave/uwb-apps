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
#if MYNEWT_VAL(CCP_ENABLED)
#include <ccp/ccp.h>
#endif
#if MYNEWT_VAL(PAN_ENABLED)
#include <pan/pan.h>
#endif
#if MYNEWT_VAL(PROVISION_ENABLED)
#include <provision/provision.h>
#endif

static void provision_postprocess(struct os_event * ev){
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);
    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *)ev->ev_arg;
    dw1000_provision_instance_t * provision = inst->provision;
    if(provision->status.provision_status == PROVISION_DONE){
        if(provision->num_node_count == 0)
            printf("No nodes found in the network\n");
        else{
            for(int i=0; i < provision->num_node_count; i++)
                printf("Provisioned with Node : %x \n",provision->dev_addr[i]);
            printf("Provision Completed \n");
            while(provision->num_node_count > 0){
                printf("Removed the node : %x \n",provision->dev_addr[provision->num_node_count-1]);
                provision_delete_node(inst,provision->dev_addr[provision->num_node_count-1]);
            }
        }
    }
    dw1000_provision_start(inst);
}

int main(int argc, char **argv){
    int rc;
    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);

    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
    inst->slot_id = MYNEWT_VAL(SLOT_ID);
    inst->dev_type = MYNEWT_VAL(DEVICE_TYPE);

#if MYNEWT_VAL(PAN_ENABLED)
    dw1000_pan_start(inst, DWT_NONBLOCKING); // Don't block on the eventq_dflt
    while(inst->pan->status.valid != true){
        os_eventq_run(os_eventq_dflt_get());
        os_cputime_delay_usecs(5000);
    }
#endif

    printf("\nTAG:%d_______Address:0x%04X\n",inst->slot_id,inst->my_short_address);

    dw1000_provision_set_postprocess(inst, &provision_postprocess);
    dw1000_provision_start(inst);
    
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}
