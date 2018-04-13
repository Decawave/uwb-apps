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
#include <lwip/raw.h>
#include <lwip/icmp.h>
#include <lwip/ethip6.h>
#include <lwip/inet_chksum.h>
#include <netif/lowpan6.h>

#define PING_ID	0xDECA
#define PING_DATA_SIZE 2
#define RX_STATUS false


static
dwt_config_t mac_config = {
	.chan = 5,			// Channel number.
	.prf = DWT_PRF_64M,		// Pulse repetition frequency.
	.txPreambLength = DWT_PLEN_256,	// Preamble length. Used in TX only.
	.rxPAC = DWT_PAC8,		// Preamble acquisition chunk size. Used in RX only.
	.txCode = 10,			// TX preamble code. Used in TX only.
	.rxCode = 10,			// RX preamble code. Used in RX only.
	.nsSFD = 0,			// 0 to use standard SFD, 1 to use non-standard SFD.
	.dataRate = DWT_BR_6M8,		// Data rate.
	.phrMode = DWT_PHRMODE_STD,	// PHY header mode.
	.sfdTO = (256 + 1 + 8 - 8)	// SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only.
};


static
dw1000_phy_txrf_config_t txrf_config = {
	.PGdly = TC_PGDELAY_CH5,
	.BOOSTNORM = dw1000_power_value(DW1000_txrf_config_9db, 5),
	.BOOSTP500 = dw1000_power_value(DW1000_txrf_config_9db, 5),
	.BOOSTP250 = dw1000_power_value(DW1000_txrf_config_9db, 5),
	.BOOSTP125 = dw1000_power_value(DW1000_txrf_config_9db, 5)
};


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

ip_addr_t tgt_ip_addr = {
	.addr[0] = MYNEWT_VAL(TGT_IP6_ADDR_1),
	.addr[1] = MYNEWT_VAL(TGT_IP6_ADDR_2),
	.addr[2] = MYNEWT_VAL(TGT_IP6_ADDR_3),
	.addr[3] = MYNEWT_VAL(TGT_IP6_ADDR_4)
};


struct netif dw1000_netif;
static struct raw_pcb *ping_pcb;
ip_addr_t ip6_tgt_addr[LWIP_IPV6_NUM_ADDRESSES];

struct pbuf *p;
void ping_prepare_echo();
err_t error;
static uint16_t seq_no = 0x0000;


int main(int argc, char **argv){
	int rc;

	dw1000_dev_instance_t * inst = hal_dw1000_inst(0);

	sysinit();
	dw1000_softreset(inst);
	inst->PANID = MYNEWT_VAL(DEVICE_PAN_ID);
	inst->my_short_address = MYNEWT_VAL(SHORT_ADDRESS);

	dw1000_set_panid(inst,inst->PANID);

	dw1000_low_level_init(inst, &txrf_config, &mac_config);
	dw1000_lwip_init(inst, &lwip_config, MYNEWT_VAL(NUM_FRAMES), MYNEWT_VAL(BUFFER_SIZE));

	dw1000_netif_config(inst, &dw1000_netif, &my_ip_addr, RX_STATUS);

	lwip_init();

	lowpan6_if_init(&dw1000_netif);

	dw1000_netif.flags |= NETIF_FLAG_UP | NETIF_FLAG_LINK_UP;
	lowpan6_set_pan_id(MYNEWT_VAL(DEVICE_PAN_ID));

	IP_ADDR6(ip6_tgt_addr, tgt_ip_addr.addr[0], tgt_ip_addr.addr[1], tgt_ip_addr.addr[2], tgt_ip_addr.addr[3]);

	ping_pcb = raw_new(IP_PROTO_ICMP);
	raw_bind(ping_pcb, dw1000_netif.ip6_addr);
	raw_connect(ping_pcb, ip6_tgt_addr);


	while (1) {
		ping_prepare_echo();
		assert(p != NULL);

		error = raw_sendto(ping_pcb, p, ip6_tgt_addr);
		if(error == ERR_OK){
			printf("\n\t[Ping Sent] ");
			printf("\n\tSeq # - %d\n\n ", seq_no);
		}

		mem_free(p);
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
		os_time_delay(5);
	}

	assert(0);
	return rc;
}

void ping_prepare_echo()
{
	size_t i;

	struct icmp_echo_hdr *iecho;
	size_t ping_size = sizeof(struct icmp_echo_hdr) + PING_DATA_SIZE;

	p = pbuf_alloc(PBUF_RAW, (u16_t)ping_size, PBUF_RAM);
	assert( p != NULL);

	iecho = (struct icmp_echo_hdr *)p->payload;
	ICMPH_TYPE_SET(iecho, ICMP_ECHO);
	ICMPH_CODE_SET(iecho, 0);
	iecho->chksum = 0;
	iecho->id     = PING_ID;
	iecho->seqno  = seq_no++;

	/* fill the additional data buffer with some data */
	for(i = 0; i < PING_DATA_SIZE; i++)
		((char*)iecho)[sizeof(struct icmp_echo_hdr) + i] = (char)(i);

	iecho->chksum = inet_chksum(iecho, ping_size);
}
