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
#include <os/mynewt.h>
#include <config/config.h>
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
#if MYNEWT_VAL(CIR_ENABLED)
#include <cir/cir.h>
#endif

#if MYNEWT_VAL(DW1000_DEVICE_0) && MYNEWT_VAL(DW1000_DEVICE_1)
#define N_DW_INSTANCES 2
#else
#define N_DW_INSTANCES 1
#endif

static int uwb_config_updated();

/* 
 * Default Config 
 */
#define LSTNR_DEFAULT_CONFIG {        \
        .acc_samples = MYNEWT_VAL(CIR_NUM_SAMPLES), \
        .verbose = "0x0" \
    }

static struct lstnr_config {
    uint16_t acc_samples_to_load;
    uint16_t verbose;
} local_conf = {0};

#define VERBOSE_CARRIER_INTEGRATOR (0x1)

static char *lstnr_get(int argc, char **argv, char *val, int val_len_max);
static int lstnr_set(int argc, char **argv, char *val);
static int lstnr_commit(void);
static int lstnr_export(void (*export_func)(char *name, char *val),
  enum conf_export_tgt tgt);

static struct lstnr_config_s {
    char acc_samples[6];
    char verbose[6];
} lstnr_config = LSTNR_DEFAULT_CONFIG;

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
    }
    return OS_ENOENT;
}

static int
lstnr_commit(void)
{
    conf_value_from_str(lstnr_config.acc_samples, CONF_INT16,
                        (void*)&(local_conf.acc_samples_to_load), 0);
    conf_value_from_str(lstnr_config.verbose, CONF_INT16,
                        (void*)&(local_conf.verbose), 0);
    uwb_config_updated();
    return 0;
}

static int
lstnr_export(void (*export_func)(char *name, char *val),
             enum conf_export_tgt tgt)
{
    export_func("lstnr/acc_samples", lstnr_config.acc_samples);
    export_func("lstnr/verbose", lstnr_config.verbose);
    return 0;
}


/* Incoming messages mempool and queue */
struct uwb_msg_hdr {
    uint32_t utime;
    uint64_t ts[N_DW_INSTANCES];
    uint16_t dlen;
    uint16_t acc_len[N_DW_INSTANCES];
    uint16_t acc_offset[N_DW_INSTANCES];
    float    pd;
    struct _dw1000_dev_rxdiag_t diag[N_DW_INSTANCES];
    int32_t  carrier_integrator;
    float    cir_fp_idx[N_DW_INSTANCES];
    float    cir_rcphase[N_DW_INSTANCES];
    float    cir_angle[N_DW_INSTANCES];
    uint64_t cir_rawts[N_DW_INSTANCES];
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

        printf("{\"utime\":%lu", hdr->utime);
        
        for(int j=0;j<N_DW_INSTANCES;j++) {
            /* Print individual data for each instance */
            printf(",\"ts%d\":%llu", j, hdr->ts[j]);
            float rssi = dw1000_calc_rssi(hal_dw1000_inst(j), &hdr->diag[j]);
            if (rssi > -200 && rssi < 100) {
#if N_DW_INSTANCES == 1
                printf(",\"rssi\":%d.%01d", (int)rssi, abs((int)(10*(rssi-(int)rssi))));
#else
                printf(",\"rssi%d\":%d.%01d", j, (int)rssi, abs((int)(10*(rssi-(int)rssi))));
#endif
            }
        }
        if (hdr->carrier_integrator && (local_conf.verbose&VERBOSE_CARRIER_INTEGRATOR)) {
            float ccor = dw1000_calc_clock_offset_ratio(hal_dw1000_inst(0), hdr->carrier_integrator);
            int ppm = (int)(ccor*1000000.0f);
            printf(",\"ccor\":%d.%03de-6",
                   ppm,
                   (int)roundf(fabsf(ccor-ppm/1000000.0f)*1000000000.0f)
                );
        }
#if MYNEWT_VAL(CIR_ENABLED)
        if (fabsf(hdr->pd) > 0.0) {
            printf(",\"pd\":");
            printf((hdr->pd < 0)?"-%d.%03d":"%d.%03d", abs((int)hdr->pd), abs((int)(1000*(hdr->pd-(int)hdr->pd))));
        }
#endif
        printf(",\"dlen\":%d", hdr->dlen);
        printf(",\"d\":\"");
        for (int i=0;i<sizeof(print_buffer) && i<hdr->dlen;i++)
        {
            printf("%02x", print_buffer[i]);
        }
        console_out('"');
#if MYNEWT_VAL(CIR_ENABLED)
        cir_complex_t *cirp;
        for(int j=0;j<N_DW_INSTANCES;j++) {
            if (hdr->acc_offset[j] > 0) {
                cirp = (cir_complex_t *) (print_buffer + hdr->acc_offset[j]);
                //float idx = ((float) hdr->diag[j].fp_idx)/64.0f;
                float idx = hdr->cir_fp_idx[j];
                float ph = hdr->cir_rcphase[j];
                float an = hdr->cir_angle[j];
                printf(",\"cir%d\":{\"o\":%d,\"fp_idx\":%d.%03d,\"rcphase\":%d.%03d,\"angle\":%d.%03d,\"rts\":%lld,\"real\":",
                       j, MYNEWT_VAL(CIR_OFFSET), (int)idx, (int)(1000*(idx-(int)idx)),
                       (int)ph, (int)fabsf((1000*(ph-(int)ph))),
                       (int)an, (int)fabsf((1000*(an-(int)an))),
                       hdr->cir_rawts[j]
                    );
                for (int i=0;i<hdr->acc_len[j];i++) {
                    printf("%c%d", (i==0)? '[':',', cirp[i].real);
                }
                printf("],\"imag\":");
                for (int i=0;i<hdr->acc_len[j];i++) {
                    printf("%c%d", (i==0)? '[':',', cirp[i].imag);
                }
                printf("]}");
            }
        }
#endif
        printf("}\n");
    end_msg:
        os_mbuf_free_chain(om);
        memset(print_buffer, 0, sizeof(print_buffer));
    }
    hal_gpio_init_out(LED_BLINK_PIN, 1);
}

static bool
rx_complete_cb(dw1000_dev_instance_t * inst, dw1000_mac_interface_t * cbs)
{
    int rc;
    struct os_mbuf *om;
    inst->control.on_error_continue_enabled = 1;

#if MYNEWT_VAL(CIR_ENABLED)
    cir_instance_t * cir[] = {
        hal_dw1000_inst(0)->cir
#if N_DW_INSTANCES > 1
        , hal_dw1000_inst(1)->cir
#endif
    };
#endif

#if N_DW_INSTANCES == 2
    /* Only use incoming data from the first instance */
    if (inst != hal_dw1000_inst(1)) {
        return true;
    }
    /* Skip packet if other dw instance doesn't have the same data in it's buffer */
    if (memcmp(hal_dw1000_inst(0)->rxbuf, hal_dw1000_inst(1)->rxbuf, hal_dw1000_inst(0)->frame_len)) {
        return false;
    }
#endif

    om = os_mbuf_get_pkthdr(&g_mbuf_pool,
                            sizeof(struct uwb_msg_hdr));
    if (om) {
        struct uwb_msg_hdr *hdr = (struct uwb_msg_hdr*)OS_MBUF_USRHDR(om);
        memset(hdr, 0, sizeof(struct uwb_msg_hdr));
        hdr->dlen = inst->frame_len;
        
        hdr->utime = os_cputime_ticks_to_usecs(os_cputime_get32());
        hdr->carrier_integrator = inst->carrier_integrator;

        for(int i=0;i<N_DW_INSTANCES;i++) {
            memcpy(&hdr->diag[i], &hal_dw1000_inst(i)->rxdiag, sizeof(struct _dw1000_dev_rxdiag_t));
            if (!hal_dw1000_inst(i)->status.lde_error) {
                hdr->ts[i] = hal_dw1000_inst(i)->rxtimestamp;
            }
        }
        rc = os_mbuf_copyinto(om, 0, inst->rxbuf, hdr->dlen);
        if (rc != 0) {
            return true;
        }

#if MYNEWT_VAL(CIR_ENABLED)
        if (N_DW_INSTANCES == 2) {
            if (cir[0]->status.valid && cir[1]->status.valid) {
                hdr->pd = cir_get_pdoa(cir[0], cir[1]);
            }
        }

        /* Do we need to load accumulator data */
        if (local_conf.acc_samples_to_load) {
            int acc_len = (local_conf.acc_samples_to_load < MYNEWT_VAL(CIR_SIZE)) ?
                local_conf.acc_samples_to_load : MYNEWT_VAL(CIR_SIZE);

            for(int i=0;i<N_DW_INSTANCES;i++) {
                if (hal_dw1000_inst(i)->cir->status.valid) {
                    hdr->cir_fp_idx[i] = cir[i]->fp_idx;
                    hdr->cir_rcphase[i] = cir[i]->rcphase;
                    hdr->cir_angle[i] = cir[i]->angle;
                    hdr->cir_rawts[i] = cir[i]->raw_ts;
                    hdr->acc_offset[i] = OS_MBUF_PKTLEN(om);
                    rc = os_mbuf_copyinto(om, hdr->acc_offset[i],
                                          (uint8_t*)hal_dw1000_inst(i)->cir->cir.array,
                                          acc_len * sizeof(cir_complex_t));
                    hdr->acc_len[i] = acc_len;
                }
            }
        }
#endif

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
    dw1000_set_rx_timeout(inst, 0);
    inst->control.on_error_continue_enabled = 1;
    dw1000_start_rx(inst);
    return true;
}
    
void
uwb_config_update(struct os_event * ev)
{
    for(int i=0;i<N_DW_INSTANCES;i++) {
        dw1000_dev_instance_t * inst = hal_dw1000_inst(i);
        dw1000_mac_config(inst, NULL);
#if MYNEWT_VAL(USE_DBLBUFFER)
        dw1000_set_dblrxbuff(inst, true);
#endif
        dw1000_set_rx_timeout(inst, 0);
        dw1000_start_rx(inst);
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

static struct uwbcfg_cbs uwb_cb = {
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
    dw1000_dev_instance_t * inst[N_DW_INSTANCES];

    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);

    uwbcfg_register(&uwb_cb);
    conf_register(&lstnr_handler);
    conf_load();
    
    for(int i=0;i<N_DW_INSTANCES;i++) {
        inst[i] = hal_dw1000_inst(i);
        inst[i]->config.rxdiag_enable = 1;
        inst[i]->config.framefilter_enabled = 0;
        inst[i]->config.bias_correction_enable = 0;
        inst[i]->config.LDE_enable = 1;
        inst[i]->config.LDO_enable = 0;
        inst[i]->config.sleep_enable = 0;
        inst[i]->config.wakeup_rx_enable = 1;
        inst[i]->config.trxoff_enable = 1;

#if MYNEWT_VAL(USE_DBLBUFFER)
        dw1000_set_dblrxbuff(inst[i], true);
        inst[i]->config.dblbuffon_enabled = 1;
        inst[i]->config.rxauto_enable = 0;
#else
        dw1000_set_dblrxbuff(inst[i], false);
        inst[i]->config.dblbuffon_enabled = 0;
        inst[i]->config.rxauto_enable = 1;
#endif
#if MYNEWT_VAL(CIR_ENABLED)
        inst[i]->config.cir_enable = (N_DW_INSTANCES>1) ? true : false;
        inst[i]->config.cir_enable = true;
#endif
        inst[i]->my_short_address = inst[i]->partID&0xffff;
        inst[i]->my_long_address = ((uint64_t) inst[i]->lotID << 33) + inst[i]->partID;
        /* Make sure to enable double buffring */
        printf("{\"device_id\"=\"%lX\"",inst[i]->device_id);
        printf(",\"PANID=\"%X\"",inst[i]->PANID);
        printf(",\"addr\"=\"%X\"",inst[i]->my_short_address);
        printf(",\"partID\"=\"%lX\"",inst[i]->partID);
        printf(",\"lotID\"=\"%lX\"",inst[i]->lotID);
        printf(",\"xtal_trim\"=\"%X\"}\n",inst[i]->xtal_trim);
        dw1000_mac_append_interface(inst[i], &g_cbs);
    }

#ifdef MYNEWT_VAL_DW1000_PDOA_SYNC
    hal_bsp_dw_clk_sync(inst, N_DW_INSTANCES);
#endif
    
    ble_init(inst[0]->my_long_address);

    for (int i=0;i<sizeof(g_mbuf_buffer)/sizeof(g_mbuf_buffer[0]);i++) {
        g_mbuf_buffer[i] = 0xdeadbeef;
    }
    create_mbuf_pool();    
    os_mqueue_init(&rxpkt_q, process_rx_data_queue, NULL);
    
    /* Start timeout-free rx on all devices */
    for(int i=0;i<N_DW_INSTANCES;i++) {
        dw1000_set_rx_timeout(inst[i], 0);
        dw1000_start_rx(inst[i]);
    }

    while (1) {
        os_eventq_run(os_eventq_dflt_get());   
    }

    assert(0);

    return rc;
}

