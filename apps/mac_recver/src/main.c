/**
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

#include <os/os.h>
#include <sysinit/sysinit.h>

#include <uwb/uwb.h>
#include <uwb/uwb_ftypes.h>
#include <uwbcfg/uwbcfg.h>

#include <mac_recver/config.h>
#include <mac_recver/wireshark.h>

typedef enum{
    STATE_SLEEP,
    STATE_RECV
}state_t;

struct uwb_msg_hdr {
    uint64_t utime;
    uint32_t ts;
    int32_t  carrier_integrator;
    uint16_t dlen;
    uint16_t diag_offset;
    uint16_t cir_offset;
};

#define MBUF_PKTHDR_OVERHEAD    sizeof(struct os_mbuf_pkthdr) + sizeof(struct uwb_msg_hdr)
#define MBUF_MEMBLOCK_OVERHEAD  sizeof(struct os_mbuf) + MBUF_PKTHDR_OVERHEAD
#define MBUF_PAYLOAD_SIZE       MYNEWT_VAL(UWB_MBUF_SIZE)
#define MBUF_BUF_SIZE           OS_ALIGN(MBUF_PAYLOAD_SIZE, 4)
#define MBUF_MEMBLOCK_SIZE      (MBUF_MEMBLOCK_OVERHEAD + MBUF_BUF_SIZE)
#define MBUF_NUM_MBUFS          MYNEWT_VAL(UWB_NUM_MBUFS)
#define MBUF_MEMPOOL_SIZE       OS_MEMPOOL_SIZE(MBUF_NUM_MBUFS, MBUF_MEMBLOCK_SIZE)

static struct dpl_callout g_rx_enable_callout;
static struct os_mqueue g_rx_pkt_q;
static state_t g_state = STATE_SLEEP;
static os_membuf_t g_mbuf_buffer[MBUF_MEMPOOL_SIZE];
static struct os_mempool g_mempool;
static struct os_mbuf_pool g_mbuf_pool;
static int g_channel;

static void
create_mbuf_pool(void)
{
    int rc;

    rc = os_mempool_init(&g_mempool, MBUF_NUM_MBUFS,
                         MBUF_MEMBLOCK_SIZE, g_mbuf_buffer, "uwb_mbuf_pool");
    assert(rc == 0);

    rc = os_mbuf_pool_init(&g_mbuf_pool, &g_mempool, MBUF_MEMBLOCK_SIZE,
                           MBUF_NUM_MBUFS);
    assert(rc == 0);
}

bool
rx_complete_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    int rc;
    struct os_mbuf *om;

    struct uwb_dev *udev = uwb_dev_idx_lookup(0);

    om = os_mbuf_get_pkthdr(&g_mbuf_pool, sizeof(struct uwb_msg_hdr));
    /* Not enough memory to handle incoming packet, drop it */
    if (!om) return true;

    struct uwb_msg_hdr *hdr = (struct uwb_msg_hdr*)OS_MBUF_USRHDR(om);
    memset(hdr, 0, sizeof(struct uwb_msg_hdr));

    hdr->utime              = os_cputime_ticks_to_usecs(os_cputime_get32());
    if (!udev->status.lde_error) hdr->ts = udev->rxtimestamp;
    hdr->carrier_integrator = inst->carrier_integrator;
    hdr->dlen               = inst->frame_len;

    rc = os_mbuf_copyinto(om, 0, inst->rxbuf, hdr->dlen);
    if (rc != 0) {
        os_mbuf_free_chain(om);
        return true;
    }
    hdr->diag_offset = hdr->dlen;

    rc = os_mbuf_copyinto(om, hdr->diag_offset, udev->rxdiag, udev->rxdiag->rxd_len);
    if (rc != 0) {
        os_mbuf_free_chain(om);
        return true;
    }

    rc = os_mqueue_put(&g_rx_pkt_q, os_eventq_dflt_get(), om);
    if (rc != 0) {
        return true;
    }

    return true;
}

static struct uwb_mac_interface uwb_mac_if = {
    .id = UWBEXT_APP0,
    .rx_complete_cb = rx_complete_cb,
};

static
void rx_enable_ev_cb(struct dpl_event *ev) {
    printf("RX enable\n");
    struct uwb_dev *udev = uwb_dev_idx_lookup(0);
    uwb_set_rx_timeout(udev, 0);
    uwb_start_rx(udev);
}

static uint8_t print_buffer[1024];
static void
process_rx_data_queue(struct os_event *ev)
{
    struct os_mbuf *om = 0;
    struct uwb_dev *udev = uwb_dev_idx_lookup(0);

    while((om = os_mqueue_get(&g_rx_pkt_q)) != NULL) {
        if(g_state == STATE_RECV){
            int payload_len = OS_MBUF_PKTLEN(om);
            payload_len = (payload_len > sizeof(print_buffer)) ? sizeof(print_buffer) : payload_len;
            struct uwb_msg_hdr *hdr = (struct uwb_msg_hdr*)(OS_MBUF_USRHDR(om));
            int rc = os_mbuf_copydata(om, 0, payload_len, print_buffer);
            if (!rc) {
                wireshark_printf("received: ");
                for (int i=0; i<sizeof(print_buffer) && i<hdr->dlen; i++)
                {
                    wireshark_printf("%02X", print_buffer[i]);
                }
                wireshark_printf("FFFF");

                struct uwb_dev_rxdiag *diag = (struct uwb_dev_rxdiag *)(print_buffer + hdr->diag_offset);
                float rssi = uwb_calc_rssi(udev, diag);

                wireshark_printf(" power: %d lqi: 0 time: %lld\r\n", (int)rssi, hdr->utime);
            }
        }
        os_mbuf_free_chain(om);
        memset(print_buffer, 0, sizeof(print_buffer));
    }
}

static void
channel_update_eventq(struct os_event * ev)
{
    printf("Channel config update\n");
    struct uwb_dev *inst = uwb_dev_idx_lookup(0);

    inst->config.channel = g_channel;
    uwb_mac_config(inst, &inst->config);
    dpl_callout_reset(&g_rx_enable_callout, 0);
}

void on_cmd(cmd_t cmd, void *arg){
    static struct os_event ev = {
        .ev_queued = 0,
        .ev_cb = channel_update_eventq,
        .ev_arg = 0};

    switch (cmd)
    {
    case CMD_CHANNEL:
        g_channel = (int)arg;
        printf("CMD_CHANNEL: %d\n", g_channel);
        os_eventq_put(os_eventq_dflt_get(), &ev);
        break;
    case CMD_SLEEP:
        g_state = STATE_SLEEP;
        printf("CMD_SLEEP\n");
        break;
    case CMD_RECV:
        g_state = STATE_RECV;
        printf("CMD_RECV\n");
        break;
    default:
        break;
    }
}

int main(int argc, char **argv){
    int rc;
    struct uwb_dev *udev;
    uint32_t utime;

    sysinit();
    app_conf_init();

    wireshark_init(on_cmd);

    create_mbuf_pool();
    os_mqueue_init(&g_rx_pkt_q, process_rx_data_queue, NULL);

    udev  = uwb_dev_idx_lookup(0);
    if (!udev) {
        printf("UWB device: not found");
        return -1;
    }
    uwb_mac_append_interface(udev, &uwb_mac_if);

    udev->config.rxdiag_enable = 1;
    udev->config.bias_correction_enable = 0;
    udev->config.LDE_enable = 1;
    udev->config.LDO_enable = 0;
    udev->config.sleep_enable = 0;
    udev->config.wakeup_rx_enable = 1;
    udev->config.trxoff_enable = 1;
    udev->config.dblbuffon_enabled = 0;
    udev->config.rxauto_enable = 1;
    uwb_mac_config(udev, &udev->config);

    dpl_callout_init(&g_rx_enable_callout, dpl_eventq_dflt_get(), rx_enable_ev_cb, NULL);
    dpl_callout_reset(&g_rx_enable_callout, 0);

    utime = dpl_cputime_ticks_to_usecs(dpl_cputime_get32());
    printf("{\"utime\": %lu,\"exe\": \"%s\"}\n",                    utime,  __FILE__);
    printf("{\"utime\": %lu,\"msg\": \"device_id    = 0x%lX\"}\n",  utime,  udev->device_id);
    printf("{\"utime\": %lu,\"msg\": \"PANID        = 0x%X\"}\n",   utime,  udev->pan_id);
    printf("{\"utime\": %lu,\"msg\": \"DeviceID     = 0x%X\"}\n",   utime,  udev->uid);
    printf("{\"utime\": %lu,\"msg\": \"partID       = 0x%lX\"}\n",  utime,  (uint32_t)(udev->euid&0xffffffff));
    printf("{\"utime\": %lu,\"msg\": \"lotID        = 0x%lX\"}\n",  utime,  (uint32_t)(udev->euid>>32));
    printf("{\"utime\": %lu,\"msg\": \"SHR_duration = %d usec\"}\n",utime,  uwb_phy_SHR_duration(udev));

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}
