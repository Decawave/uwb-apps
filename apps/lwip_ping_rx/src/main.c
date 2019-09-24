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

#include <uwb/uwb.h>
#include <uwb/uwb_ftypes.h>
#include <dw1000/dw1000_hal.h>
#include <uwb_lwip/uwb_lwip.h>

#include <lwip/init.h>
#include <lwip/ethip6.h>
#include <netif/lowpan6.h>

#define PING_ID 0xDDEE
#define RX_STATUS true

static struct os_callout rx_callout;

static
uwb_lwip_config_t lwip_config = {
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

struct ping_payload{
	uint16_t src_addr;
	uint16_t dst_addr;
	uint16_t ping_id;
	uint16_t seq_no;
	uint16_t data[5];
};

struct ping_payload *ping_pl;

static void
uwb_ev_cb(struct os_event *ev)
{
    uwb_lwip_instance_t *lwip = (uwb_lwip_instance_t *)ev->ev_arg;
    
    if (lwip->status.start_rx_error)
        printf("timer_ev_cb:[start_rx_error]\n");
    if (lwip->status.rx_error)
        printf("timer_ev_cb:[rx_error]\n");
    if (lwip->status.request_timeout){
        printf("timer_ev_cb:[request_timeout]\n");
    }
    if (lwip->status.rx_timeout_error){
        printf("timer_ev_cb:[rx_timeout_error]\n");
    }
    
    if (lwip->status.rx_error || lwip->status.request_timeout ||  lwip->status.rx_timeout_error){
        lwip->status.start_tx_error = lwip->status.rx_error = lwip->status.request_timeout = lwip->status.rx_timeout_error = 0;
    }
    
	uwb_lwip_context_t *cntxt;
	cntxt = (uwb_lwip_context_t*)lwip->lwip_netif.state;
    cntxt->rx_cb.recv(lwip, 0xffff);
    os_callout_reset(&rx_callout, OS_TICKS_PER_SEC);
}



static u8_t ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr);

int
main(int argc, char **argv){

	int rc;

	sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);

    struct uwb_dev *udev = uwb_dev_idx_lookup(0);

	udev->pan_id = MYNEWT_VAL(PANID);
	udev->my_short_address = MYNEWT_VAL(DW_DEVICE_ID_0);
	udev->fctrl_array[0] = 'L';
	udev->fctrl_array[1] = 'W';

	uwb_set_panid(udev,udev->pan_id);

	uwb_lwip_instance_t *lwip = uwb_lwip_init(udev, &lwip_config, MYNEWT_VAL(NUM_FRAMES),
                                                    MYNEWT_VAL(BUFFER_SIZE));
    uwb_netif_config(lwip, &lwip->lwip_netif, &my_ip_addr, RX_STATUS);
	lwip_init();
    lowpan6_if_init(&lwip->lwip_netif);

    lwip->dst_addr = 0x1234;

   	IP_ADDR6(ip6_tgt_addr, MYNEWT_VAL(TGT_IP6_ADDR_1), MYNEWT_VAL(TGT_IP6_ADDR_2), 
                            MYNEWT_VAL(TGT_IP6_ADDR_3), MYNEWT_VAL(TGT_IP6_ADDR_4));

    uwb_pcb_init(lwip);
    raw_recv(lwip->pcb, ping_recv, lwip);

    os_callout_init(&rx_callout, os_eventq_dflt_get(), uwb_ev_cb, lwip);
    os_callout_reset(&rx_callout, 1);
    
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
	}
    
	assert(0);
	return rc;
}


static u8_t
ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr)
{
	LWIP_UNUSED_ARG(arg);
	LWIP_UNUSED_ARG(pcb);
	LWIP_UNUSED_ARG(addr);
	LWIP_ASSERT("p != NULL", p != NULL);
	uwb_lwip_instance_t *lwip = (uwb_lwip_instance_t *)arg;
	assert(lwip->lwip_netif.state);

	if (pbuf_header(p, -PBUF_IP_HLEN)==0) {
		struct ping_payload *ping_pl = (struct ping_payload *)p->payload;

		if ((ping_pl->ping_id == PING_ID)) {
			printf("\n\t[Ping Received]\n\tSeq # - %d\n\n", ping_pl->seq_no);
			memp_free(MEMP_PBUF_POOL,p);
			return 1; /* eat the packet */
		}
	}
	memp_free(MEMP_PBUF_POOL,p);
	return 0; /* don't eat the packet */
}
