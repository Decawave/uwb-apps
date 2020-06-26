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

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <os/mynewt.h>
#include <config/config.h>
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_bsp.h"
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif

#if MYNEWT_VAL(BLE_ENABLED)
#include "bleprph/bleprph.h"
#endif

#include "console/console.h"
#include <config/config.h>

#if MYNEWT_VAL(ETH_0)
#include <lwip/tcpip.h>
#include <lwip/dhcp.h>
#include <mn_socket/mn_socket.h>
#include <inet_def_service/inet_def_service.h>

uint32_t udp_tx_addr;
uint16_t udp_tx_port;
static struct mn_socket *net_udp_socket = 0;
static struct mn_sockaddr_in net_sin;
static struct mn_sockaddr_in *net_sinp;

static void net_test_readable(void *arg, int err)
{
    /* Print any incoming data to console */
    int rc = 0;
    struct os_mbuf *m = 0;
    struct mn_sockaddr_in6 from;

    rc = mn_recvfrom(net_udp_socket, &m, (struct mn_sockaddr*)&from);
    assert(rc == 0);
    if (m) {
        uint8_t buf[32];
        int off = 0;
        int len = os_mbuf_len(m);
        int to_read;
        while (off < len) {
            to_read = (len-off < sizeof(buf))? len-off : sizeof(buf);
            os_mbuf_copydata(m, off, len, buf);
            console_write((const char*)buf, to_read);
            off += to_read;
        }
        os_mbuf_free_chain(m);
    }
}

static void net_test_writable(void *arg, int err)
{
    // Do nothing
}

static const union mn_socket_cb net_test_cbs = {
    .socket.readable = net_test_readable,
    .socket.writable = net_test_writable
};

static int
lwip_nif_up(const char *name)
{
    struct netif *nif;
    err_t err;

    nif = netif_find(name);
    if (!nif) {
        return MN_EINVAL;
    }
    if (nif->flags & NETIF_FLAG_LINK_UP) {
        netif_set_up(nif);
        netif_set_default(nif);
#if LWIP_IPV6
        nif->ip6_autoconfig_enabled = 1;
        netif_create_ip6_linklocal_address(nif, 1);
#endif
        err = dhcp_start(nif);
        return err;
    }
    return 0;
}

#endif

#include "uwbcfg/uwbcfg.h"

#include <uwb/uwb.h>
#if MYNEWT_VAL(CIR_ENABLED)
#include <cir/cir.h>

#if MYNEWT_VAL(DW1000_DEVICE_0)
#include "dw1000/dw1000_hal.h"
#include <cir_dw1000/cir_dw1000.h>
#endif
#if MYNEWT_VAL(DW3000_DEVICE_0)
#include "dw3000-c0/dw3000_hal.h"
#include <cir_dw3000-c0/cir_dw3000.h>
#endif

#endif

#if MYNEWT_VAL(UWB_DEVICE_0) && MYNEWT_VAL(UWB_DEVICE_1)
#define N_DW_INSTANCES 2
#else
#define N_DW_INSTANCES 1
#endif

static int uwb_config_updated();

static struct lstnr_config {
    uint16_t acc_samples_to_load;
    uint16_t verbose;
} local_conf = {0};

#define VERBOSE_CARRIER_INTEGRATOR (0x0001)
#define VERBOSE_RX_DIAG            (0x0002)
#define VERBOSE_CIR                (0x0004)
#define VERBOSE_NOT_TO_CONSOLE     (0x1000)

static char *lstnr_get(int argc, char **argv, char *val, int val_len_max);
static int lstnr_set(int argc, char **argv, char *val);
static int lstnr_commit(void);
static int lstnr_export(void (*export_func)(char *name, char *val),
  enum conf_export_tgt tgt);

static struct lstnr_config_s {
    char acc_samples[8];
    char verbose[8];
#if MYNEWT_VAL(ETH_0)
    char udp_tx_addr[16];
    char udp_tx_port[8];
#endif
} lstnr_config = {
    .acc_samples = MYNEWT_VAL(CIR_NUM_SAMPLES),
    .verbose = "0x0",
#if MYNEWT_VAL(ETH_0)
    .udp_tx_addr="192.168.10.255",
    .udp_tx_port="8787"
#endif
};

static struct conf_handler lstnr_handler = {
    .ch_name = "lstnr",
    .ch_get = lstnr_get,
    .ch_set = lstnr_set,
    .ch_commit = lstnr_commit,
    .ch_export = lstnr_export,
};

static char *
lstnr_get(int argc, char **argv, char *val, int val_len_max)
{
    if (argc == 1) {
        if (!strcmp(argv[0], "acc_samples"))  return lstnr_config.acc_samples;
        if (!strcmp(argv[0], "verbose"))  return lstnr_config.verbose;
#if MYNEWT_VAL(ETH_0)
        if (!strcmp(argv[0], "udp_tx_addr"))  return lstnr_config.udp_tx_addr;
        if (!strcmp(argv[0], "udp_tx_port"))  return lstnr_config.udp_tx_port;
#endif
    }
    return NULL;
}

static int
lstnr_set(int argc, char **argv, char *val)
{
    if (argc == 1) {
        if (!strcmp(argv[0], "acc_samples")) {
            return CONF_VALUE_SET(val, CONF_STRING, lstnr_config.acc_samples);
        }
        if (!strcmp(argv[0], "verbose")) {
            return CONF_VALUE_SET(val, CONF_STRING, lstnr_config.verbose);
        }
#if MYNEWT_VAL(ETH_0)
        if (!strcmp(argv[0], "udp_tx_addr")) {
            return CONF_VALUE_SET(val, CONF_STRING, lstnr_config.udp_tx_addr);
        }
        if (!strcmp(argv[0], "udp_tx_port")) {
            return CONF_VALUE_SET(val, CONF_STRING, lstnr_config.udp_tx_port);
        }
#endif
    }
    return OS_ENOENT;
}

static int
lstnr_commit(void)
{
    conf_value_from_str(lstnr_config.acc_samples, CONF_INT16,
                        (void*)&(local_conf.acc_samples_to_load), 0);
#if MYNEWT_VAL(CIR_ENABLED)
    if (local_conf.acc_samples_to_load > MYNEWT_VAL(CIR_MAX_SIZE)) {
        local_conf.acc_samples_to_load = MYNEWT_VAL(CIR_MAX_SIZE);
    }
#endif
    conf_value_from_str(lstnr_config.verbose, CONF_INT16,
                        (void*)&(local_conf.verbose), 0);

#if MYNEWT_VAL(ETH_0)
    if (mn_inet_pton(MN_AF_INET, lstnr_config.udp_tx_addr, &udp_tx_addr) != 1) {
        console_printf("Invalid udp address %s\n", lstnr_config.udp_tx_addr);
        return 0;
    }
    conf_value_from_str(lstnr_config.udp_tx_port, CONF_INT16,
                        (void*)&(udp_tx_port), 0);

    memset(&net_sin, 0, sizeof(net_sin));
    net_sin.msin_len = sizeof(net_sin);
    net_sin.msin_family = MN_AF_INET;
    net_sin.msin_port = htons(udp_tx_port);
    net_sin.msin_addr.s_addr = udp_tx_addr;
    net_sinp = &net_sin;
#endif

    uwb_config_updated();
    return 0;
}

static int
lstnr_export(void (*export_func)(char *name, char *val),
             enum conf_export_tgt tgt)
{
    export_func("lstnr/acc_samples", lstnr_config.acc_samples);
    export_func("lstnr/verbose", lstnr_config.verbose);
#if MYNEWT_VAL(ETH_0)
    export_func("lstnr/udp_tx_addr", lstnr_config.udp_tx_addr);
    export_func("lstnr/udp_tx_port", lstnr_config.udp_tx_port);
#endif
    return 0;
}


/* Incoming messages mempool and queue */
struct uwb_msg_hdr {
    uint32_t utime;
    uint16_t dlen;
    uint16_t diag_offset;
    uint16_t cir_offset;
    int32_t  carrier_integrator;
#ifdef MYNEWT_VAL_PDOA_SPI_NUM_INSTANCES
    uint64_t ts[MYNEWT_VAL(PDOA_SPI_NUM_INSTANCES)];
    struct _dw1000_dev_rxdiag_t diag[MYNEWT_VAL(PDOA_SPI_NUM_INSTANCES)];
#else
    uint64_t ts[N_DW_INSTANCES];
#endif
#ifdef MYNEWT_VAL_PDOA_SPI_NUM_INSTANCES
    float    pd[MYNEWT_VAL(PDOA_SPI_NUM_INSTANCES)-1];
#else
    float    pd[1];
#endif
};

#ifndef MYNEWT_VAL_PDOA_SPI_NUM_INSTANCES
static struct {
    union {
        struct uwb_dev_rxdiag diag;
        uint8_t diag_storage[MYNEWT_VAL(UWB_DEV_RXDIAG_MAXLEN)];
    };
} tmp_diag[N_DW_INSTANCES];
#endif

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

static struct dpl_callout rx_reenable_callout;

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

static char output_buffer[512];
static int
mprintf(struct os_mbuf *m, const char *fmt, ...)
{
    int rc = 0;
    va_list args;
    int num_chars;

    va_start(args, fmt);
    num_chars = vsnprintf(output_buffer, sizeof(output_buffer)-10, fmt, args);
    if (m) {
        rc = os_mbuf_append(m, output_buffer, num_chars);
        assert(rc==0);
    }
    if ((local_conf.verbose&VERBOSE_NOT_TO_CONSOLE)==0) {
        console_write(output_buffer, num_chars);
    }
    va_end(args);
    return num_chars;
}

#if MYNEWT_VAL(CIR_ENABLED)
#if MYNEWT_VAL(DW1000_DEVICE_0)
static struct cir_dw1000_instance tmp_cir;
#endif
#if MYNEWT_VAL(DW3000_DEVICE_0)
static struct cir_dw3000_instance tmp_cir;
#endif
#endif
static uint8_t print_buffer[1024];
static uint8_t diag_buffer[1024];
static void
process_rx_data_queue(struct os_event *ev)
{
    int rc;
    struct os_mbuf *m = 0;
    struct os_mbuf *om = 0;
    struct uwb_msg_hdr *hdr;
    int payload_len;
    struct uwb_dev *udev = uwb_dev_idx_lookup(0);
    uint64_t ts;
#ifdef MYNEWT_VAL_PDOA_SPI_NUM_INSTANCES
    int n_instances = MYNEWT_VAL(PDOA_SPI_NUM_INSTANCES);
#else
    int n_instances = N_DW_INSTANCES;
#endif

    hal_gpio_init_out(LED_BLINK_PIN, 0);
    while ((om = os_mqueue_get(&rxpkt_q)) != NULL) {
        hdr = (struct uwb_msg_hdr*)(OS_MBUF_USRHDR(om));

        payload_len = OS_MBUF_PKTLEN(om);
        payload_len = (payload_len > sizeof(print_buffer)) ? sizeof(print_buffer) :
            payload_len;

        rc = os_mbuf_copydata(om, 0, payload_len, print_buffer);
        if (rc) {
            goto end_msg;
        }

        m = os_msys_get_pkthdr(16, 0);
        if (!m) {
            goto end_msg;
        }

        mprintf(m,"{\"utime\":%lu", hdr->utime);

        mprintf(m,",\"ts\":[");
        for(int j=0;j<n_instances;j++) {
            ts = hdr->ts[j];
            mprintf(m,"%s%llu", (j==0)?"":",", ts);
        }
        mprintf(m,"]");

#ifndef MYNEWT_VAL_PDOA_SPI_NUM_INSTANCES
        rc = os_mbuf_copydata(om, hdr->dlen + hdr->diag_offset, sizeof(struct uwb_dev_rxdiag), &tmp_diag);
        struct uwb_dev_rxdiag *diag = (struct uwb_dev_rxdiag *)&tmp_diag;
        int offset = 0;
        for(int j=0;j<n_instances;j++) {
            rc = os_mbuf_copydata(om, hdr->dlen + hdr->diag_offset + offset, diag->rxd_len, &tmp_diag[j]);
            offset += diag->rxd_len;
        }
#endif

        if ((local_conf.verbose&VERBOSE_RX_DIAG)) {
            mprintf(m,",\"rssi\":[");
            for(int j=0;j<n_instances;j++) {
#ifdef MYNEWT_VAL_PDOA_SPI_NUM_INSTANCES
                struct uwb_dev_rxdiag *diag = &hdr->diag[j].diag;
#else
                rc = os_mbuf_copydata(om, hdr->dlen + hdr->diag_offset + j*MYNEWT_VAL(UWB_DEV_RXDIAG_MAXLEN), sizeof(struct uwb_dev_rxdiag), diag_buffer);
                struct uwb_dev_rxdiag *diag = (struct uwb_dev_rxdiag *)diag_buffer;
                rc = os_mbuf_copydata(om, hdr->dlen + hdr->diag_offset + j*diag->rxd_len, diag->rxd_len, diag_buffer);
#endif

                float rssi = uwb_calc_rssi(udev, diag);
                if (rssi > -200 && rssi < 100) {
                    mprintf(m,"%s%d.%01d", (j==0)?"":",",
                           (int)rssi, abs((int)(10*(rssi-(int)rssi))));
                } else {
                    mprintf(m,"%snull", (j==0)?"":",");
                }
            }
            mprintf(m,"],\"fppl\":[");
            for(int j=0;j<n_instances;j++) {
#ifdef MYNEWT_VAL_PDOA_SPI_NUM_INSTANCES
                struct uwb_dev_rxdiag *diag = &hdr->diag[j].diag;
#else
                rc = os_mbuf_copydata(om, hdr->dlen + hdr->diag_offset + j*MYNEWT_VAL(UWB_DEV_RXDIAG_MAXLEN), sizeof(struct uwb_dev_rxdiag), diag_buffer);
                struct uwb_dev_rxdiag *diag = (struct uwb_dev_rxdiag *)diag_buffer;
                rc = os_mbuf_copydata(om, hdr->dlen + hdr->diag_offset + j*diag->rxd_len, diag->rxd_len, diag_buffer);
#endif
                float fppl = uwb_calc_fppl(udev, diag);
                if (fppl > -200 && fppl < 100) {
                    mprintf(m,"%s%d.%01d", (j==0)?"":",",
                           (int)fppl, abs((int)(10*(fppl-(int)fppl))));
                } else {
                    mprintf(m,"%snull", (j==0)?"":",");
                }
            }
            mprintf(m,"]");
        }
        if (hdr->carrier_integrator && (local_conf.verbose&VERBOSE_CARRIER_INTEGRATOR)) {
            float ccor = uwb_calc_clock_offset_ratio(udev, hdr->carrier_integrator, UWB_CR_CARRIER_INTEGRATOR);

            int ppm = (int)(ccor*1000000.0f);
            mprintf(m,",\"ccor\":%d.%03de-6",
                   ppm,
                   (int)roundf(fabsf(ccor-ppm/1000000.0f)*1000000000.0f)
                );
        }
        mprintf(m,",\"pd\":[");
        if (udev->capabilities.single_receiver_pdoa) {
            int j=0;
#ifdef MYNEWT_VAL_PDOA_SPI_NUM_INSTANCES
                struct uwb_dev_rxdiag *diag = &hdr->diag[j].diag;
#else
                rc = os_mbuf_copydata(om, hdr->dlen + hdr->diag_offset + j*MYNEWT_VAL(UWB_DEV_RXDIAG_MAXLEN), sizeof(struct uwb_dev_rxdiag), diag_buffer);
                struct uwb_dev_rxdiag *diag = (struct uwb_dev_rxdiag *)diag_buffer;
                rc = os_mbuf_copydata(om, hdr->dlen + hdr->diag_offset + j*diag->rxd_len, diag->rxd_len, diag_buffer);
#endif

            float pdoa = uwb_calc_pdoa(udev, diag);
            if (isnan(pdoa)) {
                /* Json can't handle Nan, but it can handle null */
                mprintf(m,"%snull", (j==0)?"":",");
            } else {
                mprintf(m,(pdoa < 0)?"%s-%d.%03d":"%s%d.%03d",
                        (j==0)?"":",",
                        abs((int)pdoa), abs((int)(1000*(pdoa-(int)pdoa))));
            }
        }

        for(int j=0;j<n_instances-1;j++) {
            if (isnan(hdr->pd[j])) {
                /* Json can't handle Nan, but it can handle null */
                mprintf(m,"%snull", (j==0)?"":",");
            } else {
                mprintf(m,(hdr->pd[j] < 0)?"%s-%d.%03d":"%s%d.%03d",
                        (j==0)?"":",",
                        abs((int)hdr->pd[j]), abs((int)(1000*(hdr->pd[j]-(int)hdr->pd[j]))));
            }
        }

        mprintf(m,"],\"dlen\":%d", hdr->dlen);
        mprintf(m,",\"d\":\"");
        for (int i=0;i<sizeof(print_buffer) && i<hdr->dlen;i++)
        {
            mprintf(m,"%02x", print_buffer[i]);
        }
        mprintf(m,"\"");
#if MYNEWT_VAL(CIR_ENABLED)
        if (local_conf.verbose&VERBOSE_CIR) {
            mprintf(m,",\"cir\":[");
            for(int j=0;j<n_instances;j++) {
#if MYNEWT_VAL(DW1000_DEVICE_0)
                rc = os_mbuf_copydata(om, hdr->dlen + j*sizeof(struct cir_dw1000_instance), sizeof(struct cir_dw1000_instance), &tmp_cir);
                struct cir_dw1000_instance* cirp = &tmp_cir;
#endif
#if MYNEWT_VAL(DW3000_DEVICE_0)
                rc = os_mbuf_copydata(om, hdr->dlen + j*sizeof(struct cir_dw3000_instance), sizeof(struct cir_dw3000_instance), &tmp_cir);
                struct cir_dw3000_instance* cirp = &tmp_cir;
#endif
                if (cirp->cir_inst.status.valid > 0 || 1) {
                    //float idx = ((float) hdr->diag[j].fp_idx)/64.0f;
                    float idx = cirp->fp_idx;
                    float ph = cirp->rcphase;
                    float an = cirp->angle;
                    mprintf(m,"%s{\"o\":%d,\"fp_idx\":%d.%03d,\"rcphase\":%d.%03d,\"angle\":%d.%03d,\"rts\":%lld",
                           (j==0)?"":",", cirp->offset, (int)idx, (int)(1000*(idx-(int)idx)),
                           (int)ph, (int)fabsf((1000*(ph-(int)ph))),
                           (int)an, (int)fabsf((1000*(an-(int)an))),
                           cirp->raw_ts
                        );
                    // mprintf(m,",\"fp_amp\":[%d,%d,%d]", hdr->diag[j].fp_amp, hdr->diag[j].fp_amp2, hdr->diag[j].fp_amp3);
                    // mprintf(m,",\"cir_pwr\":%d", hdr->diag[j].cir_pwr);
                    // mprintf(m,",\"rx_std\":%d", hdr->diag[j].rx_std);
                    if (local_conf.acc_samples_to_load) {
                        mprintf(m,",\"real\":[");
                        for (int i=0;i<local_conf.acc_samples_to_load;i++) {
                            mprintf(m,"%s%d", (i==0)? "":",", (int)cirp->cir.array[i].real);
                        }
                        mprintf(m,"],\"imag\":[");
                        for (int i=0;i<local_conf.acc_samples_to_load;i++) {
                            mprintf(m,"%s%d", (i==0)? "":",", (int)cirp->cir.array[i].imag);
                        }
                        mprintf(m,"]");
                    }
                    mprintf(m,"}");
                }
            }
            mprintf(m,"]");
        }
#endif  // CIR_ENABLED
        mprintf(m,"}");
        if ((local_conf.verbose&VERBOSE_NOT_TO_CONSOLE)==0) {
            console_out('\n');
        }
#if MYNEWT_VAL(ETH_0)
        /* Setup UDP broadcast socket */
        if (!net_udp_socket) {
            rc = mn_socket(&net_udp_socket, MN_PF_INET, MN_SOCK_DGRAM, 0);
            mn_socket_set_cbs(net_udp_socket, NULL, &net_test_cbs);
        }
        if (m) {
            rc = mn_sendto(net_udp_socket, m, (struct mn_sockaddr *)net_sinp);
            if (rc != 0) {
                printf("sendto: %d\n", rc);
                os_mbuf_free_chain(m);
            }
        }
#else
        os_mbuf_free_chain(m);
#endif
    end_msg:
        os_mbuf_free_chain(om);
        memset(print_buffer, 0, sizeof(print_buffer));
    }
    hal_gpio_init_out(LED_BLINK_PIN, 1);
}

static bool
rx_complete_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    int rc;
    struct os_mbuf *om;
#if MYNEWT_VAL(CIR_ENABLED)
    struct uwb_dev *udev[N_DW_INSTANCES];
    for(int i=0;i<N_DW_INSTANCES;i++) {
        udev[i] = uwb_dev_idx_lookup(i);
    }

    struct cir_instance * cir[] = {
        udev[0]->cir
#if N_DW_INSTANCES > 1
        , udev[1]->cir
#endif
    };
#endif

#if N_DW_INSTANCES == 2
    /* Only use incoming data from the last instance */
    if (inst != udev[1]) {
        if (!inst->status.rx_restarted) {
            dpl_callout_reset(&rx_reenable_callout, DPL_TICKS_PER_SEC/100);
        }
        return true;
    }
#if MYNEWT_VAL(CIR_ENABLED)
    dpl_callout_stop(&rx_reenable_callout);
    /* Restart receivers */
    for(int i=0;i<N_DW_INSTANCES;i++) {
        uwb_set_rx_timeout(udev[i], 0xffff);
        uwb_set_rxauto_disable(udev[i], MYNEWT_VAL(CIR_ENABLED));
        uwb_start_rx(udev[i]);
    }
#endif
    /* Skip packet if other dw instance doesn't have the same data in it's buffer */
    if (memcmp(udev[0]->rxbuf, udev[1]->rxbuf, udev[0]->frame_len)) {
        return true;
    }
#endif

    om = os_mbuf_get_pkthdr(&g_mbuf_pool, sizeof(struct uwb_msg_hdr));
    if (!om) {
        /* Not enough memory to handle incoming packet, drop it */
        return true;
    }

    struct uwb_msg_hdr *hdr = (struct uwb_msg_hdr*)OS_MBUF_USRHDR(om);
    memset(hdr, 0, sizeof(struct uwb_msg_hdr));
    hdr->dlen = inst->frame_len;
    hdr->utime = os_cputime_ticks_to_usecs(os_cputime_get32());
    hdr->carrier_integrator = inst->carrier_integrator;
    uint16_t offset = 0;
    rc = os_mbuf_copyinto(om, offset, inst->rxbuf, hdr->dlen);
    if (rc != 0) {
        os_mbuf_free_chain(om);
        return true;
    }
    offset += hdr->dlen;
    hdr->diag_offset = offset;

#ifdef MYNEWT_VAL_PDOA_SPI_NUM_INSTANCES
    for(int i=0;i<MYNEWT_VAL(PDOA_SPI_NUM_INSTANCES);i++) {
        struct pdoa_cir_data *pdata = hal_bsp_get_pdoa_cir_data(i);
        memcpy(&hdr->diag[i], &pdata->rxdiag, sizeof(hdr->diag[i]));
        hdr->ts[i] = pdata->ts;
    }
#else
    for(int i=0;i<N_DW_INSTANCES;i++) {
        struct uwb_dev * udev = uwb_dev_idx_lookup(i);

        rc = os_mbuf_copyinto(om, offset, udev->rxdiag, udev->rxdiag->rxd_len);
        if (rc != 0) {
            os_mbuf_free_chain(om);
            return true;
        }
        offset += udev->rxdiag->rxd_len;

        if (!udev->status.lde_error) {
            hdr->ts[i] = udev->rxtimestamp;
        }
    }
#endif

#if MYNEWT_VAL(CIR_ENABLED)
    hdr->cir_offset = offset;

#ifdef MYNEWT_VAL_PDOA_SPI_NUM_INSTANCES
    struct pdoa_cir_data *pdata0 = hal_bsp_get_pdoa_cir_data(0);
    for(int j=1;j<MYNEWT_VAL(PDOA_SPI_NUM_INSTANCES);j++) {
        struct pdoa_cir_data *pdata = hal_bsp_get_pdoa_cir_data(j);
        /* Verify crc and data length */
        if (pdata0->rxbuf_crc16 != pdata->rxbuf_crc16 || pdata0->rx_length != pdata->rx_length) {
            hdr->pd[j-1] = nanf("");
            continue;
        }

        /* Verify that we're measuring the same leading edge */
        int raw_ts_diff = (pdata->cir.raw_ts - cir[0]->raw_ts)/64;
        float fp_diff = roundf(pdata->cir.fp_idx) - roundf(cir[0]->fp_idx) + raw_ts_diff;
        if (fabsf(fp_diff) > 1.0) {
            hdr->pd[j-1] = nanf("");
            continue;
        }

        if (cir[0]->status.valid && pdata->cir.status.valid) {
            hdr->pd[j-1] = cir_get_pdoa(cir[0], &pdata->cir);
        }
    }
#else
    if (N_DW_INSTANCES == 2) {
        if (cir[0]->status.valid && cir[1]->status.valid) {
            hdr->pd[0] = cir_get_pdoa(cir[0], cir[1]);
        }
    }
#endif

    /* Do we need to load accumulator data */
    if (local_conf.acc_samples_to_load) {
#if MYNEWT_VAL(DW1000_DEVICE_0)
        struct cir_dw1000_instance *src;
#endif
#if MYNEWT_VAL(DW3000_DEVICE_0)
        struct cir_dw3000_instance *src;
#endif
#ifdef MYNEWT_VAL_PDOA_SPI_NUM_INSTANCES
        for(int i=0;i<MYNEWT_VAL(PDOA_SPI_NUM_INSTANCES);i++) {
            if (i==0) {
                src = hal_dw1000_inst(i)->cir;
            } else {
                struct pdoa_cir_data *pdata = hal_bsp_get_pdoa_cir_data(i);
                src = &pdata->cir;
            }
#else // MYNEWT_VAL_PDOA_SPI_NUM_INSTANCES
        for(int i=0;i<N_DW_INSTANCES;i++) {

#if MYNEWT_VAL(DW1000_DEVICE_0)
            src = hal_dw1000_inst(i)->cir;
#endif
#if MYNEWT_VAL(DW3000_DEVICE_0)
            src = hal_dw3000_inst(i)->cir;
#endif

#endif // MYNEWT_VAL_PDOA_SPI_NUM_INSTANCES
            rc = os_mbuf_copyinto(om, hdr->dlen + i*sizeof(*src),
                                  src, sizeof(*src));
            assert(rc == 0);
        }
    }
#endif // CIR_ENABLED

    rc = os_mqueue_put(&rxpkt_q, os_eventq_dflt_get(), om);
    if (rc != 0) {
        return true;
    }

    return true;
}

bool
error_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    return true;
}

bool
timeout_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    /* Restart receivers */
    for(int i=0;i<N_DW_INSTANCES;i++) {
        struct uwb_dev *udev = uwb_dev_idx_lookup(i);
        uwb_set_rx_timeout(udev, 0);
#if N_DW_INSTANCES > 1
        uwb_set_rxauto_disable(udev, MYNEWT_VAL(CIR_ENABLED));
#endif
        uwb_start_rx(udev);
    }
    return true;
}

bool
reset_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    return true;
}

void
uwb_config_update(struct os_event * ev)
{
    for(int i=0;i<N_DW_INSTANCES;i++) {
        struct uwb_dev *inst = uwb_dev_idx_lookup(i);
        uwb_mac_config(inst, NULL);
        uwb_txrf_config(inst, &inst->config.txrf);
#if MYNEWT_VAL(USE_DBLBUFFER)
        uwb_set_dblrxbuff(inst, true);
#endif
        uwb_set_rx_timeout(inst, 0);
#if N_DW_INSTANCES > 1
        uwb_set_rxauto_disable(inst, MYNEWT_VAL(CIR_ENABLED));
#endif
        inst->config.rxdiag_enable = (local_conf.verbose&VERBOSE_RX_DIAG) != 0;
        uwb_start_rx(inst);
    }
}

static int
uwb_config_updated()
{
    static struct os_event ev = {
        .ev_queued = 0,
        .ev_cb = uwb_config_update,
        .ev_arg = 0};
    os_eventq_put(os_eventq_dflt_get(), &ev);

    return 0;
}

static
void rx_reenable_ev_cb(struct dpl_event *ev) {
    /* Restart receivers */
    for(int i=0;i<N_DW_INSTANCES;i++) {
        struct uwb_dev *udev = uwb_dev_idx_lookup(i);
        uwb_set_rx_timeout(udev, 0);
#if N_DW_INSTANCES > 1
        uwb_set_rxauto_disable(udev, MYNEWT_VAL(CIR_ENABLED));
#endif
        uwb_start_rx(udev);
    }
}

static struct uwbcfg_cbs uwb_cb = {
    .uc_update = uwb_config_updated
};

static struct uwb_mac_interface g_cbs = {
    .id = UWBEXT_APP0,
    .rx_complete_cb = rx_complete_cb,
    .rx_timeout_cb = timeout_cb,
    .rx_error_cb = error_cb,
    .tx_error_cb = 0,
    .tx_complete_cb = 0,
    .reset_cb = reset_cb
};

int main(int argc, char **argv){
    int rc;
    struct uwb_dev *udev[N_DW_INSTANCES];

    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);

    /* Load any saved uwb settings */
    uwbcfg_register(&uwb_cb);
    conf_register(&lstnr_handler);
    conf_load();

#if MYNEWT_VAL(ETH_0)
    /* Bring up network device with dhcp */
    rc = lwip_nif_up("st1");
    assert(rc==0);
#endif

    for(int i=0;i<N_DW_INSTANCES;i++) {
        udev[i] = uwb_dev_idx_lookup(i);
        udev[i]->config.rxdiag_enable = (local_conf.verbose&VERBOSE_RX_DIAG) != 0;
        udev[i]->config.bias_correction_enable = 0;
        udev[i]->config.LDE_enable = 1;
        udev[i]->config.LDO_enable = 0;
        udev[i]->config.sleep_enable = 0;
        udev[i]->config.wakeup_rx_enable = 1;
        udev[i]->config.trxoff_enable = 1;

#if MYNEWT_VAL(USE_DBLBUFFER)
        /* Make sure to enable double buffring */
        udev[i]->config.dblbuffon_enabled = 1;
        udev[i]->config.rxauto_enable = 0;
        uwb_set_dblrxbuff(udev[i], true);
#else
        udev[i]->config.dblbuffon_enabled = 0;
        udev[i]->config.rxauto_enable = 1;
        uwb_set_dblrxbuff(udev[i], false);
#endif
#if MYNEWT_VAL(CIR_ENABLED)
        udev[i]->config.cir_enable = (N_DW_INSTANCES>1) ? true : false;
        udev[i]->config.cir_enable = true;
#endif
        printf("{\"device_id\"=\"%lX\"",udev[i]->device_id);
        printf(",\"PANID=\"%X\"",udev[i]->pan_id);
        printf(",\"addr\"=\"%X\"",udev[i]->uid);
        printf(",\"partID\"=\"%lX\"",(uint32_t)(udev[i]->euid&0xffffffff));
        printf(",\"lotID\"=\"%lX\"}\n",(uint32_t)(udev[i]->euid>>32));
        uwb_mac_append_interface(udev[i], &g_cbs);
    }

    /* Sync clocks if available */
    if (uwb_sync_to_ext_clock(udev[0]).ext_sync || 1) {
        printf("{\"ext_sync\"=\"");
        for(int i=0;i<N_DW_INSTANCES;i++) {
            printf("%d", udev[i]->status.ext_sync);
        }
        printf("\"}\n");
    }

#if MYNEWT_VAL(BLE_ENABLED)
    ble_init(udev[0]->euid);
#endif

    /* Prepare memory buffer */
    for (int i=0;i<sizeof(g_mbuf_buffer)/sizeof(g_mbuf_buffer[0]);i++) {
        g_mbuf_buffer[i] = 0xdeadbeef;
    }
    create_mbuf_pool();
    os_mqueue_init(&rxpkt_q, process_rx_data_queue, NULL);
    dpl_callout_init(&rx_reenable_callout, dpl_eventq_dflt_get(), rx_reenable_ev_cb, NULL);

    /* Start timeout-free rx on all devices */
    for(int i=0;i<N_DW_INSTANCES;i++) {
        uwb_set_rx_timeout(udev[i], 0);
#if N_DW_INSTANCES > 1
        uwb_set_rxauto_disable(udev[i], MYNEWT_VAL(CIR_ENABLED));
#endif
        uwb_start_rx(udev[i]);
    }

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    assert(0);

    return rc;
}
