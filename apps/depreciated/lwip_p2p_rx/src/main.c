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
#include <lwip/dw1000_lwip.h>
#endif

#if MYNEWT_VAL(DW1000_LWIP_P2P)
#include <dw1000_lwip_p2p.h>
#define RX_STATUS false
#endif

#if MYNEWT_VAL(DW1000_LWIP_P2P)
static dw1000_lwip_config_t lwip_config = {
    .poll_resp_delay = 0x4800,
    .resp_timeout = 0xF000,
    .uwbtime_to_systime = 0
};

ip_addr_t ip6_tgt_addr[LWIP_IPV6_NUM_ADDRESSES];

ip_addr_t my_ip_addr = {
    .addr[0] = MYNEWT_VAL(DEV_IP6_ADDR_1),
    .addr[1] = MYNEWT_VAL(DEV_IP6_ADDR_2),
    .addr[2] = MYNEWT_VAL(DEV_IP6_ADDR_3),
    .addr[3] = MYNEWT_VAL(DEV_IP6_ADDR_4)
};
#endif


#if MYNEWT_VAL(DW1000_LWIP_P2P)
dw1000_lwip_p2p_payload_info_t payload_info[1];

static dw1000_lwip_p2p_node_address_t master_node_addr[1] = {
    [0] = {
        .node_addr = 0x1234,
    },
};

#endif

#if MYNEWT_VAL(DW1000_LWIP_P2P)
#define FRAME_LEN   10
void lwip_p2p_postprocess(struct os_event * ev){
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);


    hal_gpio_toggle(LED_BLINK_PIN);
    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *)ev->ev_arg;
    char * rx_frame = (char *)inst->lwip->lwip_p2p->payload_info[0]->input_payload.payload_ptr;
    printf("%s Received\n",rx_frame);

    dw1000_lwip_start_rx(inst,0);
}
#endif

int main(int argc, char **argv){
    int rc;
    
    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);
    
    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);

    inst->PANID = 0xDECA;
    inst->my_short_address = MYNEWT_VAL(DW_DEVICE_ID_0);
    inst->my_long_address = ((uint64_t) inst->device_id << 32) + inst->partID;

    dw1000_set_panid(inst,inst->PANID);
    dw1000_set_address16(inst, inst->my_short_address);

    dw1000_mac_init(inst, NULL);
#if MYNEWT_VAL(DW1000_CLOCK_CALIBRATION)
    dw1000_ccp_init(inst, 2, MYNEWT_VAL(UUID_CCP_MASTER));
#endif

#if MYNEWT_VAL(DW1000_LWIP)
    dw1000_lwip_init(inst, &lwip_config, MYNEWT_VAL(NUM_FRAMES), MYNEWT_VAL(BUFFER_SIZE));
    dw1000_netif_config(inst, &inst->lwip->lwip_netif, &my_ip_addr, RX_STATUS);
    lwip_init();
    lowpan6_if_init(&inst->lwip->lwip_netif);

    inst->lwip->lwip_netif.flags |= NETIF_FLAG_UP | NETIF_FLAG_BROADCAST | NETIF_FLAG_LINK_UP;
    lowpan6_set_pan_id(MYNEWT_VAL(DEVICE_PAN_ID));

    dw1000_pcb_init(inst);
#endif

#if MYNEWT_VAL(DW1000_LWIP_P2P)
    IP_ADDR6(payload_info[0].ip_addr, MYNEWT_VAL(TGT_IP6_ADDR_1), MYNEWT_VAL(TGT_IP6_ADDR_2), 
                            MYNEWT_VAL(TGT_IP6_ADDR_3), MYNEWT_VAL(TGT_IP6_ADDR_4));

    payload_info[0].output_payload.payload_ptr = (void *)malloc(sizeof(char) * FRAME_LEN);
    payload_info[0].input_payload.payload_ptr = (void *)malloc(sizeof(char) * FRAME_LEN);

    assert(payload_info[0].input_payload.payload_ptr != NULL);
    assert(payload_info[0].output_payload.payload_ptr != NULL);
    payload_info[0].output_payload.payload_size = sizeof(char) * FRAME_LEN;
    payload_info[0].input_payload.payload_size = sizeof(char) * FRAME_LEN;

    dw1000_lwip_p2p_init(inst, 2, master_node_addr, payload_info);
    inst->lwip->dst_addr = 0x1234;
    dw1000_lwip_p2p_set_postprocess(inst, &lwip_p2p_postprocess);
#endif

    printf("device_id = 0x%lX\n",inst->device_id);
    printf("PANID = 0x%X\n",inst->PANID);
    printf("DeviceID = 0x%X\n",inst->my_short_address);
    printf("partID = 0x%lX\n",inst->partID);
    printf("lotID = 0x%lX\n",inst->lotID);
    printf("xtal_trim = 0x%X\n",inst->xtal_trim);

    dw1000_lwip_start_rx(inst,0);
    
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}
