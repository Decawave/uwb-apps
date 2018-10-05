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

#include <lwip/init.h>
#include <lwip/ethip6.h>
#include <netif/lowpan6.h>

#define PING_ID	0xDDEE
#define RX_STATUS false

static
dw1000_lwip_config_t lwip_config = {
	.poll_resp_delay = 0x4800,
	.resp_timeout = 0xF000,
	.uwbtime_to_systime = 0
};


ip_addr_t my_ip_addr = {
	.addr[0] = MYNEWT_VAL(DEV_IP6_ADDR_1),
	.addr[1] = MYNEWT_VAL(DEV_IP6_ADDR_2),
	.addr[2] = MYNEWT_VAL(DEV_IP6_ADDR_3),
	.addr[3] = MYNEWT_VAL(DEV_IP6_ADDR_4)
};


ip_addr_t ip6_tgt_addr[4];

err_t error;
static uint16_t seq_no = 0x0000;
char *payload;

struct ping_payload{
	uint16_t src_addr;
	uint16_t dst_addr;
	uint16_t ping_id;
	uint16_t seq_no;
	uint16_t data[5];
};

static struct os_callout blinky_callout;

#define SAMPLE_FREQ 20.0

static void timer_ev_cb(struct os_event *ev) {
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);

    hal_gpio_toggle(LED_BLINK_PIN);
    
    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *)ev->ev_arg;
	uint16_t payload_size = (uint16_t)sizeof(struct ping_payload);

    os_callout_reset(&blinky_callout, OS_TICKS_PER_SEC/SAMPLE_FREQ);

	struct ping_payload *ping_pl = (struct ping_payload*)payload;
	ping_pl->src_addr = MYNEWT_VAL(SHORT_ADDRESS);
	ping_pl->dst_addr = 0x4321;
	ping_pl->ping_id = PING_ID;
	ping_pl->seq_no  = seq_no++;

	dw1000_lwip_send(inst, payload_size, payload, ip6_tgt_addr);

	printf("\n\tSeq # - %d\n\n", seq_no);

	if (inst->lwip->status.start_rx_error)
		printf("timer_ev_cb:[start_rx_error]\n");
	if (inst->lwip->status.start_tx_error)
		printf("timer_ev_cb:[start_tx_error]\n");
	if (inst->lwip->status.rx_error)
		printf("timer_ev_cb:[rx_error]\n");
	if (inst->lwip->status.request_timeout){
		printf("timer_ev_cb:[request_timeout]\n");
		error = ERR_INPROGRESS;
	}
	if (inst->lwip->status.rx_timeout_error){
		printf("timer_ev_cb:[rx_timeout_error]\n");
		error = ERR_TIMEOUT;
	}

	if (inst->lwip->status.start_tx_error ||
			inst->lwip->status.rx_error ||
			inst->lwip->status.request_timeout ||
			inst->lwip->status.rx_timeout_error){

		inst->lwip->status.start_tx_error = inst->lwip->status.rx_error = inst->lwip->status.request_timeout = inst->lwip->status.rx_timeout_error = 0;
	}
	print_error(error);
}

static void init_timer(dw1000_dev_instance_t * inst) {
    os_callout_init(&blinky_callout, os_eventq_dflt_get(), timer_ev_cb, inst);
    os_callout_reset(&blinky_callout, OS_TICKS_PER_SEC);
}

int main(int argc, char **argv){
	int rc;

	dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
	
	sysinit();

	inst->PANID = MYNEWT_VAL(DEVICE_PAN_ID);
	inst->my_short_address = MYNEWT_VAL(SHORT_ADDRESS);

	dw1000_set_panid(inst,inst->PANID);
	dw1000_low_level_init(inst, NULL, NULL);
	dw1000_lwip_init(inst, &lwip_config, MYNEWT_VAL(NUM_FRAMES), MYNEWT_VAL(BUFFER_SIZE));
    dw1000_netif_config(inst, &inst->lwip->lwip_netif, &my_ip_addr, RX_STATUS);
	lwip_init();
    lowpan6_if_init(&inst->lwip->lwip_netif);

    inst->lwip->lwip_netif.flags |= NETIF_FLAG_UP | NETIF_FLAG_LINK_UP;
	lowpan6_set_pan_id(MYNEWT_VAL(DEVICE_PAN_ID));

    dw1000_pcb_init(inst);

    inst->lwip->dst_addr = 0x4321;

    IP_ADDR6(ip6_tgt_addr, MYNEWT_VAL(TGT_IP6_ADDR_1), MYNEWT_VAL(TGT_IP6_ADDR_2), 
                            MYNEWT_VAL(TGT_IP6_ADDR_3), MYNEWT_VAL(TGT_IP6_ADDR_4));


    payload = (char *)malloc(sizeof(struct ping_payload));
    assert(payload != NULL);

    init_timer(inst);

	while (1) {
        os_eventq_run(os_eventq_dflt_get());
	}

	assert(0);
	return rc;
}