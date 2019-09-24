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

#include <dw1000/dw1000_hal.h>
#include <uwb/uwb.h>
#include <uwb/uwb_ftypes.h>

#if MYNEWT_VAL(UWB_RNG_ENABLED)
#include <uwb_rng/uwb_rng.h>
#endif
#if MYNEWT_VAL(TWR_DS_EXT_ENABLED)
#include <twr_ds_ext/twr_ds_ext.h>
#endif
#if MYNEWT_VAL(TDMA_ENABLED)
#include <tdma/tdma.h>
#endif
#if MYNEWT_VAL(UWB_CCP_ENABLED)
#include <uwb_ccp/uwb_ccp.h>
#endif
#if MYNEWT_VAL(UWB_WCS_ENABLED)
#include <uwb_wcs/uwb_wcs.h>
#endif
#if MYNEWT_VAL(TIMESCALE)
#include <timescale/timescale.h> 
#endif

static void master_slot_ev_cb(struct dpl_event * ev);
static void slave_slot_ev_cb(struct dpl_event * ev);

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
master_slot_ev_cb(struct dpl_event * ev){
    assert(ev);

    tdma_slot_t * slot = (tdma_slot_t *) dpl_event_get_arg(ev);
    tdma_instance_t * tdma = slot->parent;
    struct uwb_rng_instance * rng = (struct uwb_rng_instance *)slot->arg;
    struct uwb_dev * inst = tdma->dev_inst;
    uint16_t idx = slot->idx;

    uwb_set_delay_start(inst, tdma_rx_slot_start(tdma, idx));
    uint16_t timeout = uwb_phy_frame_duration(inst, sizeof(ieee_rng_response_frame_t))
                            + rng->config.tx_holdoff_delay;  // Remote side turn around time.
                            
    uwb_set_rx_timeout(inst, timeout);
    uwb_rng_listen(rng, UWB_BLOCKING);
}


static void 
slave_slot_ev_cb(struct dpl_event *ev){
    assert(ev);

    tdma_slot_t * slot = (tdma_slot_t *) dpl_event_get_arg(ev);
    tdma_instance_t * tdma = slot->parent;
    struct uwb_rng_instance * rng = (struct uwb_rng_instance *)slot->arg;

    uint16_t idx = slot->idx;
    
    uint64_t dx_time = tdma_tx_slot_start(tdma, idx);
    dx_time = dx_time & 0xFFFFFFFFFE00UL;
  
    switch (idx%4){
        case 0:uwb_rng_request_delay_start(rng, 0x4321, dx_time, DWT_SS_TWR);
        break;
        case 1:uwb_rng_request_delay_start(rng, 0x4321, dx_time, DWT_SS_TWR_EXT);
        break;
        case 2:uwb_rng_request_delay_start(rng, 0x4321, dx_time, DWT_DS_TWR);
        break;
        case 3:uwb_rng_request_delay_start(rng, 0x4321, dx_time, DWT_DS_TWR_EXT);
        break;
        default:break;
    }

}




int main(int argc, char **argv){
    int rc;
    
    dw1000_dev_instance_t * inst[] = {
        hal_dw1000_inst(0),
        hal_dw1000_inst(1)
    };
    struct uwb_dev *udev[] = {
        uwb_dev_idx_lookup(0),
        uwb_dev_idx_lookup(1)
    };

    udev[0]->config.txrf = udev[1]->config.txrf = (struct uwb_dev_txrf_config){
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
    printf("{\"utime\": %lu,\"msg\": \"request_duration = %d usec\"}\n", utime, uwb_phy_frame_duration(udev[0], sizeof(ieee_rng_request_frame_t) )); 
    printf("{\"utime\": %lu,\"msg\": \"response_duration = %d usec\"}\n",utime, uwb_phy_frame_duration(udev[0], sizeof(ieee_rng_response_frame_t) )); 
    printf("{\"utime\": %lu,\"msg\": \"SHR_duration = %d usec\"}\n",utime, uwb_phy_SHR_duration(udev[0])); 

    struct uwb_rng_instance * rng0 = (struct uwb_rng_instance *) uwb_mac_find_cb_inst_ptr(udev[0], UWBEXT_RNG);
    struct uwb_rng_instance * rng1 = (struct uwb_rng_instance *) uwb_mac_find_cb_inst_ptr(udev[1], UWBEXT_RNG);
    printf("{\"utime\": %lu,\"msg\": \"holdoff = %d usec\"}\n",utime, (uint16_t)ceilf(uwb_dwt_usecs_to_usecs(rng0->config.tx_holdoff_delay) ));
    
    uwb_ccp_start(uwb_mac_find_cb_inst_ptr(udev[0], UWBEXT_CCP), CCP_ROLE_MASTER);
    uwb_ccp_start(uwb_mac_find_cb_inst_ptr(udev[1], UWBEXT_CCP), CCP_ROLE_SLAVE);

    // Using GPIO5 and GPIO6 to study timing.
    dw1000_gpio5_config_ext_txe(inst[0]);
    dw1000_gpio5_config_ext_txe(inst[1]);
    dw1000_gpio6_config_ext_rxe(inst[0]);
    dw1000_gpio6_config_ext_rxe(inst[1]);

    struct _tdma_instance_t *tdma0 = (struct _tdma_instance_t *) uwb_mac_find_cb_inst_ptr(udev[0], UWBEXT_TDMA);
    struct _tdma_instance_t *tdma1 = (struct _tdma_instance_t *) uwb_mac_find_cb_inst_ptr(udev[1], UWBEXT_TDMA);

    for (uint16_t i = 3; i < MYNEWT_VAL(TDMA_NSLOTS); i+=1){
        tdma_assign_slot(tdma0, master_slot_ev_cb, i, (void*)rng0);
        tdma_assign_slot(tdma1, slave_slot_ev_cb, i, (void*)rng1);
    }
    
    clk_sync(inst, 2);
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}

