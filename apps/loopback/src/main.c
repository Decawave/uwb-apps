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

#include <dw1000/dw1000_dev.h>
#include <dw1000/dw1000_hal.h>
#include <dw1000/dw1000_phy.h>
#include <dw1000/dw1000_mac.h>
#include <dw1000/dw1000_ftypes.h>

#if MYNEWT_VAL(RNG_ENABLED)
#include <rng/rng.h>
#endif
#if MYNEWT_VAL(TWR_DS_EXT_ENABLED)
#include <twr_ds_ext/twr_ds_ext.h>
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

static void master_slot_ev_cb(struct os_event * ev);
static void slave_slot_ev_cb(struct os_event * ev);

#define NINST 2

static void 
clk_sync(dw1000_dev_instance_t * inst[], uint8_t n){

    for (uint8_t i = 0; i < n; i++ )
        dw1000_phy_external_sync(inst[i],33, true);

    hal_gpio_init_out(MYNEWT_VAL(DW1000_PDOA_SYNC_EN), 1);
    hal_gpio_init_out(MYNEWT_VAL(DW1000_PDOA_SYNC_CLR), 1);

    hal_gpio_init_out(MYNEWT_VAL(DW1000_PDOA_SYNC), 1);
    os_cputime_delay_usecs(1000);
    hal_gpio_write(MYNEWT_VAL(DW1000_PDOA_SYNC), 0);

    hal_gpio_write(MYNEWT_VAL(DW1000_PDOA_SYNC_CLR), 0);
    hal_gpio_write(MYNEWT_VAL(DW1000_PDOA_SYNC_EN), 0);
    
    for (uint8_t i = 0; i < n; i++ )
        dw1000_phy_external_sync(inst[i],0, false);
}
   
static void 
master_slot_ev_cb(struct os_event * ev){
    assert(ev);

    tdma_slot_t * slot = (tdma_slot_t *) ev->ev_arg;
    tdma_instance_t * tdma = slot->parent;
    dw1000_dev_instance_t * inst = tdma->dev_inst;
    dw1000_rng_instance_t * rng = (dw1000_rng_instance_t *) dw1000_mac_find_cb_inst_ptr(inst, DW1000_RNG);
    uint16_t idx = slot->idx;

    dw1000_set_delay_start(inst, tdma_rx_slot_start(tdma, idx));
    uint16_t timeout = dw1000_phy_frame_duration(&inst->attrib, sizeof(ieee_rng_response_frame_t))                 
                            + rng->config.tx_holdoff_delay;         // Remote side turn arroud time. 
                            
    dw1000_set_rx_timeout(inst, timeout);
    dw1000_rng_listen(rng, DWT_BLOCKING);
}


static void 
slave_slot_ev_cb(struct os_event *ev){
    assert(ev);

    tdma_slot_t * slot = (tdma_slot_t *) ev->ev_arg;
    tdma_instance_t * tdma = slot->parent;
    dw1000_dev_instance_t * inst = tdma->dev_inst;
    dw1000_rng_instance_t * rng = (dw1000_rng_instance_t *) dw1000_mac_find_cb_inst_ptr(inst, DW1000_RNG);

    uint16_t idx = slot->idx;
    
    uint64_t dx_time = tdma_tx_slot_start(tdma, idx);
    dx_time = dx_time & 0xFFFFFFFFFE00UL;
  
    dw1000_rng_request_delay_start(rng, 0x4321, dx_time, DWT_SS_TWR);

}




int main(int argc, char **argv){
    int rc;
    
    dw1000_dev_instance_t * inst[] = {
        hal_dw1000_inst(0),
        hal_dw1000_inst(1)
    };

    inst[0]->config.txrf = inst[1]->config.txrf = (struct _dw1000_dev_txrf_config_t ){
                    .PGdly = TC_PGDELAY_CH5,
                    .BOOSTNORM = dw1000_power_value(DW1000_txrf_config_0db, 0.5),
                    .BOOSTP500 = dw1000_power_value(DW1000_txrf_config_0db, 0.5),
                    .BOOSTP250 = dw1000_power_value(DW1000_txrf_config_0db, 0.5),
                    .BOOSTP125 = dw1000_power_value(DW1000_txrf_config_0db, 0.5)   
                };
    sysinit();

    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);

    uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
    printf("{\"utime\": %lu,\"msg\": \"request_duration = %d usec\"}\n", utime, dw1000_phy_frame_duration(&inst[0]->attrib, sizeof(ieee_rng_request_frame_t) )); 
    printf("{\"utime\": %lu,\"msg\": \"response_duration = %d usec\"}\n",utime, dw1000_phy_frame_duration(&inst[0]->attrib, sizeof(ieee_rng_response_frame_t) )); 
    printf("{\"utime\": %lu,\"msg\": \"SHR_duration = %d usec\"}\n",utime, dw1000_phy_SHR_duration(&inst[0]->attrib)); 
    dw1000_rng_instance_t * rng = (dw1000_rng_instance_t *) dw1000_mac_find_cb_inst_ptr(inst[0], DW1000_RNG);
    printf("{\"utime\": %lu,\"msg\": \"holdoff = %d usec\"}\n",utime, (uint16_t)ceilf(dw1000_dwt_usecs_to_usecs(rng->config.tx_holdoff_delay) ));
    
    dw1000_ccp_start(dw1000_mac_find_cb_inst_ptr(inst[0], DW1000_CCP), CCP_ROLE_MASTER);
    dw1000_ccp_start(dw1000_mac_find_cb_inst_ptr(inst[1], DW1000_CCP), CCP_ROLE_SLAVE);

    // Using GPIO5 and GPIO6 to study timing.
    dw1000_gpio5_config_ext_txe(inst[0]);
    dw1000_gpio5_config_ext_txe(inst[1]);
    dw1000_gpio6_config_ext_rxe(inst[0]);
    dw1000_gpio6_config_ext_rxe(inst[1]);

    for (uint16_t i = 3; i < MYNEWT_VAL(TDMA_NSLOTS) - 8; i+=8){
        tdma_assign_slot(dw1000_mac_find_cb_inst_ptr(inst[0], DW1000_TDMA), master_slot_ev_cb, i, NULL);
        tdma_assign_slot(dw1000_mac_find_cb_inst_ptr(inst[1], DW1000_TDMA), slave_slot_ev_cb, i, NULL);
    }
    
    clk_sync(inst, 2);
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}

