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

#include "bleprph.h"

#include "console/console.h"
#include <config/config.h>
#include "uwbcfg/uwbcfg.h"

#include "dw1000/dw1000_ftypes.h"
#include "dw1000/dw1000_dev.h"
#include "dw1000/dw1000_hal.h"
#include "dw1000/dw1000_phy.h"
#include "dw1000/dw1000_mac.h"

/* Incoming messages mempool and queue */
struct uwb_msg_hdr {
    uint64_t ts;
    uint16_t dlen;
    struct _dw1000_dev_rxdiag_t diag;
};

#define MBUF_PKTHDR_OVERHEAD    sizeof(struct os_mbuf_pkthdr) + sizeof(struct uwb_msg_hdr)
#define MBUF_MEMBLOCK_OVERHEAD  sizeof(struct os_mbuf) + MBUF_PKTHDR_OVERHEAD

#define MBUF_NUM_MBUFS      MYNEWT_VAL(UWB_NUM_MBUFS)
#define MBUF_PAYLOAD_SIZE   MYNEWT_VAL(UWB_MBUF_SIZE)
#define MBUF_BUF_SIZE       OS_ALIGN(MBUF_PAYLOAD_SIZE, 4)
#define MBUF_MEMBLOCK_SIZE  (MBUF_BUF_SIZE + MBUF_MEMBLOCK_OVERHEAD)
#define MBUF_MEMPOOL_SIZE   OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

static struct os_mbuf_pool g_mbuf_pool;
static struct os_mempool g_mbuf_mempool;
static os_membuf_t g_mbuf_buffer[MBUF_MEMPOOL_SIZE];
static struct os_mqueue rxpkt_q;

static void
create_mbuf_pool(void)
{
    int rc;

    rc = os_mempool_init(&g_mbuf_mempool, MBUF_NUM_MBUFS,
                         MBUF_MEMBLOCK_SIZE, &g_mbuf_buffer[0], "uwb_mbuf_pool");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&g_mbuf_pool, &g_mbuf_mempool, MBUF_MEMBLOCK_SIZE,
                           MBUF_NUM_MBUFS);
    assert(rc == 0);
}

static uint8_t print_buffer[1024];
static void
process_rx_data_queue(struct os_event *ev)
{
    int rc;
    struct os_mbuf *om;
    struct uwb_msg_hdr *hdr;
    int payload_len;

    hal_gpio_init_out(LED_BLINK_PIN, 0);
    while ((om = os_mqueue_get(&rxpkt_q)) != NULL) { 
        hdr = (struct uwb_msg_hdr*)(OS_MBUF_USRHDR(om));

        payload_len = OS_MBUF_PKTLEN(om);
        payload_len = (payload_len > sizeof(print_buffer)) ? sizeof(print_buffer) :
            payload_len;

        rc = os_mbuf_copydata(om, 0, payload_len,
                              print_buffer);
        if (rc)
        {
            goto end_msg;
        }

        float rssi = dw1000_calc_rssi(hal_dw1000_inst(0), &hdr->diag);
        printf("{\"ts\":%llu,\"utime\":%lu", hdr->ts,
               os_cputime_ticks_to_usecs(os_cputime_get32()));
        
        if (rssi > -200 && rssi < 100) {
            printf(",\"rssi\":\"%d.%d\"",(int)rssi, abs((int)(10*(rssi-(int)rssi))));
        }
        printf(",\"dlen\":%d", hdr->dlen);
        printf(",\"d\":\"");
        for (int i=0;i<sizeof(print_buffer) && i<hdr->dlen;i++)
        {
            printf("%02x", print_buffer[i]);
        }
        printf("\"}\n");
    end_msg:
        os_mbuf_free_chain(om);
        memset(print_buffer, 0, sizeof(print_buffer));
    }
    hal_gpio_init_out(LED_BLINK_PIN, 1);
}


static uint8_t buffer[1024];
static bool
rx_complete_cb(dw1000_dev_instance_t * inst, dw1000_mac_interface_t * cbs)
{
    int rc;
    struct os_mbuf *om;
    inst->control.on_error_continue_enabled = 1;

    
    om = os_mbuf_get_pkthdr(&g_mbuf_pool,
                            sizeof(struct uwb_msg_hdr));
    if (om) {
        struct uwb_msg_hdr *hdr = (struct uwb_msg_hdr*)OS_MBUF_USRHDR(om);
        memset(hdr, 0, sizeof(struct uwb_msg_hdr));
        hdr->dlen = (inst->frame_len>sizeof(buffer))? sizeof(buffer) : inst->frame_len;
        
        dw1000_read_rx(inst, buffer, 0, hdr->dlen);

        /* Verify that the timestamp is valid */
        if(inst->sys_status & SYS_STATUS_LDEDONE) {
            hdr->ts = dw1000_read_rxtime(inst);
        } else if (inst->sys_status & SYS_STATUS_LDEERR) {
            hdr->ts = 0;
        } else {
            /* Should never happen */
            assert(0);
        }

        dw1000_read(inst, RX_TIME_ID, 2,
                    (uint8_t*)&(hdr->diag.fp_amp),
                    sizeof(uint16_t));
        dw1000_read(inst, RX_FQUAL_ID, 2,
                    (uint8_t*)&(hdr->diag.fp_amp2),
                    sizeof(uint16_t)*3);
        hdr->diag.pacc_cnt =
            (dw1000_read_reg(inst, RX_FINFO_ID, 2, sizeof(uint16_t)) & 0xFFF0) >> 4;
        
        rc = os_mbuf_copyinto(om, 0, buffer, hdr->dlen);
        if (rc != 0) {
            return true;
        }
        rc = os_mqueue_put(&rxpkt_q, os_eventq_dflt_get(), om);
        if (rc != 0) {
            return true;
        }
    } else {
        /* Not enough memory to handle incoming packet, drop it */
    }
    return true;
}

bool
error_cb(dw1000_dev_instance_t * inst, dw1000_mac_interface_t * cbs)
{
    printf("err_cb\n");
    dw1000_set_dblrxbuff(inst, false);
    dw1000_set_dblrxbuff(inst, true);
    dw1000_set_rx_timeout(inst, 0);
    inst->control.on_error_continue_enabled = 1;
    dw1000_start_rx(inst);
    return true;
}
    
void
uwb_config_update(struct os_event * ev)
{
    dw1000_dev_instance_t * inst = ev->ev_arg;
    dw1000_set_dblrxbuff(inst, false);
    dw1000_mac_config(inst, NULL);

    dw1000_set_dblrxbuff(inst, true);
    dw1000_set_rx_timeout(inst, 0);
    dw1000_start_rx(inst);
}

int
uwb_config_updated()
{
    static struct os_event ev = {
        .ev_queued = 0,
        .ev_cb = uwb_config_update,
        .ev_arg = 0};
    ev.ev_arg = (void*)hal_dw1000_inst(0);
    os_eventq_put(os_eventq_dflt_get(), &ev);

    return 0;
}

struct uwbcfg_cbs uwb_cb = {
    .uc_update = uwb_config_updated
};

static dw1000_mac_interface_t g_cbs = {
    .id = 72,
    .rx_complete_cb = rx_complete_cb,
    .rx_error_cb = error_cb,
    .tx_error_cb = error_cb,
    .tx_complete_cb = 0,
    .reset_cb = error_cb
};

int main(int argc, char **argv){
    int rc;

    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);

    uwbcfg_register(&uwb_cb);
    conf_load();
    
    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
    inst->config.dblbuffon_enabled = 1;
    inst->config.framefilter_enabled = 0;
    inst->config.bias_correction_enable = 0;
    inst->config.LDE_enable = 1;
    inst->config.LDO_enable = 0;
    inst->config.sleep_enable = 0;
    inst->config.wakeup_rx_enable = 1;
    inst->config.rxauto_enable = 1;
    inst->control.on_error_continue_enabled = 1;

    inst->my_short_address = inst->partID&0xffff;
    inst->my_long_address = ((uint64_t) inst->lotID << 33) + inst->partID;
    dw1000_mac_append_interface(inst, &g_cbs);
    
    printf("{\"device_id\"=\"%lX\"",inst->device_id);
    printf(",\"PANID=\"%X\"",inst->PANID);
    printf(",\"addr\"=\"%X\"",inst->my_short_address);
    printf(",\"partID\"=\"%lX\"",inst->partID);
    printf(",\"lotID\"=\"%lX\"",inst->lotID);
    printf(",\"slot_id\"=\"%d\"",inst->slot_id);
    printf(",\"xtal_trim\"=\"%X\"}\n",inst->xtal_trim);

    ble_init(inst->my_long_address);

    for (int i=0;i<sizeof(g_mbuf_buffer)/sizeof(g_mbuf_buffer[0]);i++) {
        g_mbuf_buffer[i] = 0xdeadbeef;
    }
    create_mbuf_pool();    
    os_mqueue_init(&rxpkt_q, process_rx_data_queue, NULL);
    
    dw1000_set_rx_timeout(inst, 0);
    dw1000_start_rx(inst);
    
    while (1) {
        os_eventq_run(os_eventq_dflt_get());   
    }

    assert(0);

    return rc;
}

