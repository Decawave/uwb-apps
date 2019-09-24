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
#include <float.h>
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

#include <config/config.h>
#include "uwbcfg/uwbcfg.h"

#if MYNEWT_VAL(RNG_ENABLED)
#include <rng/rng.h>
#endif
#if MYNEWT_VAL(TDMA_ENABLED)
#include <tdma/tdma.h>
#endif
#if MYNEWT_VAL(CCP_ENABLED)
#include <ccp/ccp.h>
#endif
#if MYNEWT_VAL(WCS_ENABLED)
#include <wcs/wcs.h>
#endif
#if MYNEWT_VAL(DW1000_LWIP)
#include <lwip/lwip.h>
#endif
#if MYNEWT_VAL(TIMESCALE)
#include <timescale/timescale.h> 
#endif
#include "json_encode.h"

//#define DIAGMSG(s,u) printf(s,u)
#ifndef DIAGMSG
#define DIAGMSG(s,u)
#endif

static bool dw1000_config_updated = false;
static void slot_complete_cb(struct os_event *ev);
void prph_init(char *);
static uint16_t g_slot[MYNEWT_VAL(TDMA_NSLOTS)] = {0};


cir_t g_cir;



/*! 
 * @fn complete_cb(dw1000_dev_instance_t * inst, dw1000_mac_interface_t * cbs)
 *
 * @brief This callback is part of the  dw1000_mac_interface_t extension interface and invoked of the completion of a range request. 
 * The dw1000_mac_interface_t is in the interrupt context and is used to schedule events an event queue. Processing should be kept 
 * to a minimum giving the interrupt context. All algorithms activities should be deferred to a thread on an event queue. 
 * The callback should return true if and only if it can determine if it is the sole recipient of this event. 
 *
 * NOTE: The MAC extension interface is a link-list of callbacks, subsequent callbacks on the list will be not be called in the 
 * event of returning true. 
 *
 * @param inst  - dw1000_dev_instance_t *
 * @param cbs   - dw1000_mac_interface_t *
 *
 * output parameters
 *
 * returns bool
 */
/* The timer callout */

static struct os_callout slot_callout;
uint16_t g_idx_latest;

static bool
complete_cb(dw1000_dev_instance_t * inst, dw1000_mac_interface_t * cbs){
    if(inst->fctrl != FCNTL_IEEE_RANGE_16){
        return false;
    }
    dw1000_rng_instance_t * rng = inst->rng; 
    g_idx_latest = (rng->idx)%rng->nframes; // Store valid frame pointer
    os_callout_init(&slot_callout, os_eventq_dflt_get(), slot_complete_cb, inst);
    os_eventq_put(os_eventq_dflt_get(), &slot_callout.c_ev);
    return true;
}

/*! 
 * @fn slot_complete_cb(struct os_event * ev)
 *
 * @brief In the example this function represents the event context processing of the received range request. 
 * In this case, a JSON string is constructed and written to stdio. See the ./apps/matlab or ./apps/python folders for examples on 
 * how to parse and render these results. 
 * 
 * input parameters
 * @param inst - struct os_event *  
 * output parameters
 * returns none 
 */

static void slot_complete_cb(struct os_event *ev)
{
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);
  
    hal_gpio_toggle(LED_BLINK_PIN);
    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *)ev->ev_arg;
    dw1000_rng_instance_t * rng = inst->rng; 
    twr_frame_t * frame = rng->frames[(g_idx_latest)%rng->nframes];

    if (frame->code == DWT_DS_TWR_T2 || frame->code ==  DWT_DS_TWR_FINAL || frame->code == DWT_DS_TWR_EXT_T2 || frame->code == DWT_DS_TWR_EXT_FINAL) {
        float time_of_flight = dw1000_rng_twr_to_tof(rng, g_idx_latest);
        uint32_t utime =os_cputime_ticks_to_usecs(os_cputime_get32()); 
        float rssi = dw1000_get_rssi(inst);

        printf("{\"utime\": %lu,\"tof\": %lu,\"range\": %lu,\"azimuth\": %lu,\"res_tra\":\"%lX\","
                    " \"rec_tra\":\"%lX\", \"rssi\":%lu}\n",
                utime,
                *(uint32_t *)(&time_of_flight), 
                *(uint32_t *)(&frame->spherical.range),
                *(uint32_t *)(&frame->spherical.azimuth),
                (frame->response_timestamp - frame->request_timestamp),
                (frame->transmission_timestamp - frame->reception_timestamp),
                *(uint32_t *)(&rssi)
        );
        //json_cir_encode(&g_cir, utime, "cir", CIR_SIZE);
        frame->code = DWT_DS_TWR_END;
    }    
    else if (frame->code == DWT_SS_TWR_FINAL) {
        float time_of_flight = dw1000_rng_twr_to_tof(rng,g_idx_latest);
        float range = dw1000_rng_tof_to_meters(time_of_flight);
        uint32_t utime =os_cputime_ticks_to_usecs(os_cputime_get32()); 
        float rssi = dw1000_get_rssi(inst);
 
        printf("{\"utime\": %lu,\"tof\": %lu,\"range\": %lu,\"res_tra\":\"%lX\","
                    " \"rec_tra\":\"%lX\", \"rssi\":%lu}\n",
                utime,
                *(uint32_t *)(&time_of_flight), 
                *(uint32_t *)(&range),
                (frame->response_timestamp - frame->request_timestamp),
                (frame->transmission_timestamp - frame->reception_timestamp),
                *(uint32_t *)(&rssi)
        );
        frame->code = DWT_SS_TWR_END;
    }
}

/*! 
 * @fn slot_cb(struct os_event * ev)
 * 
 * In this example, slot_cb schedules the turning of the transceiver to receive mode at the desired epoch. 
 * The precise timing is under the control of the DW1000, and the dw_time variable defines this epoch. 
 * Note the slot_cb itself is scheduled MYNEWT_VAL(OS_LATENCY) in advance of the epoch to set up the transceiver accordingly.
 * 
 * Note: The epoch time is referenced to the Rmarker symbol, it is therefore necessary to advance the rxtime by the SHR_duration such 
 * that the preamble are received. The transceiver, when transmitting adjust the txtime accordingly.
 *
 * input parameters
 * @param inst - struct os_event *  
 *
 * output parameters
 *
 * returns none 
 */
   
static void 
slot_cb(struct os_event * ev){
    assert(ev);

    tdma_slot_t * slot = (tdma_slot_t *) ev->ev_arg;
    tdma_instance_t * tdma = slot->parent;
    dw1000_dev_instance_t * inst = tdma->parent;
    dw1000_ccp_instance_t * ccp = inst->ccp;
    uint16_t idx = slot->idx;

    if (dw1000_config_updated) {
        dw1000_mac_config(inst, NULL);
        dw1000_phy_config_txrf(inst, &inst->config.txrf);
        dw1000_config_updated = false;
    }

    dw1000_set_delay_start(inst, tdma_rx_slot_start(inst, idx));
    uint16_t timeout = dw1000_phy_frame_duration(&inst->attrib, sizeof(ieee_rng_response_frame_t))                 
                            + inst->rng->config.rx_timeout_delay;         // Remote side turn around time.
    dw1000_set_rx_timeout(inst, timeout);
    dw1000_rng_listen(inst, DWT_BLOCKING);
}

/**
 * @fn uwb_config_update
 * 
 * Called from the main event queue as a result of the uwbcfg packet
 * having received a commit/load of new uwb configuration.
 */
int
uwb_config_updated()
{
    dw1000_config_updated = true;
    return 0;
}

struct uwbcfg_cbs uwb_cb = {
    .uc_update = uwb_config_updated
};


int main(int argc, char **argv){
    int rc;

    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);

    /* Register callback for UWB configuration changes */
    
    uwbcfg_register(&uwb_cb);
    /* Load config from flash */
    conf_load();

    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
    char name[32]={0};
    sprintf(name,"%X-%04X",inst->PANID,inst->my_short_address);
    prph_init(name);
    dw1000_mac_interface_t cbs = (dw1000_mac_interface_t){
        .id =  DW1000_APP0,
        .complete_cb = complete_cb
    };
    dw1000_mac_append_interface(inst, &cbs);

#if MYNEWT_VAL(CCP_ENABLED)
    dw1000_ccp_start(inst, CCP_ROLE_MASTER);
#endif
    uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
    printf("{\"utime\": %lu,\"exce\":\"%s\"}\n",utime,__FILE__);
    printf("{\"utime\": %lu,\"msg\": \"device_id = 0x%lX\"}\n",utime,inst->device_id);
    printf("{\"utime\": %lu,\"msg\": \"PANID = 0x%X\"}\n",utime,inst->PANID);
    printf("{\"utime\": %lu,\"msg\": \"DeviceID = 0x%X\"}\n",utime,inst->my_short_address);
    printf("{\"utime\": %lu,\"msg\": \"partID = 0x%lX\"}\n",utime,inst->partID);
    printf("{\"utime\": %lu,\"msg\": \"lotID = 0x%lX\"}\n",utime,inst->lotID);
    printf("{\"utime\": %lu,\"msg\": \"xtal_trim = 0x%X\"}\n",utime,inst->xtal_trim);  
    printf("{\"utime\": %lu,\"msg\": \"frame_duration = %d usec\"}\n",utime,dw1000_phy_frame_duration(&inst->attrib, sizeof(twr_frame_final_t))); 
    printf("{\"utime\": %lu,\"msg\": \"SHR_duration = %d usec\"}\n",utime,dw1000_phy_SHR_duration(&inst->attrib)); 
    printf("{\"utime\": %lu,\"msg\": \"holdoff = %d usec\"}\n",utime,(uint16_t)ceilf(dw1000_dwt_usecs_to_usecs(inst->rng->config.tx_holdoff_delay))); 


    for (uint16_t i = 0; i < sizeof(g_slot)/sizeof(uint16_t); i++)
        g_slot[i] = i;

    for (uint16_t i = 2; i < sizeof(g_slot)/sizeof(uint16_t); i++)
        tdma_assign_slot(inst->tdma, slot_cb,  g_slot[i], &g_slot[i]);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}

