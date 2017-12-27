/**
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

#include "dw1000/dw1000_dev.h"
#include "dw1000/dw1000_hal.h"
#include "dw1000/dw1000_phy.h"
#include "dw1000/dw1000_mac.h"
#include "dw1000/dw1000_rng.h"
#include <dw1000/dw1000_lwip.h>
#include "dw1000/dw1000_ftypes.h"

static dwt_config_t mac_config = {
    .chan = 5,                          // Channel number. 
    .prf = DWT_PRF_64M,                 // Pulse repetition frequency. 
    .txPreambLength = DWT_PLEN_128,     // Preamble length. Used in TX only. 
    .rxPAC = DWT_PAC8,                  // Preamble acquisition chunk size. Used in RX only. 
    .txCode = 9,                        // TX preamble code. Used in TX only. 
    .rxCode = 8,                        // RX preamble code. Used in RX only. 
    .nsSFD = 0,                         // 0 to use standard SFD, 1 to use non-standard SFD. 
    .dataRate = DWT_BR_6M8,             // Data rate. 
    .phrMode = DWT_PHRMODE_STD,         // PHY header mode. 
    .sfdTO = (129 + 8 - 8)              // SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. 
};

static dw1000_phy_txrf_config_t txrf_config = { 
        .PGdly = 0xC0,
        //.power = 0x25456585
        .BOOSTNORM = dw1000_power_value(DW1000_txrf_config_0db, 3),
        .BOOSTP500 = dw1000_power_value(DW1000_txrf_config_0db, 3),    
        .BOOSTP250 = dw1000_power_value(DW1000_txrf_config_0db, 3),     
        .BOOSTP125 = dw1000_power_value(DW1000_txrf_config_0db, 3)
};

static dw1000_rng_config_t rng_config = {
    .tx_holdoff_delay = 0x800,          // Send Time delay in usec.
    .rx_timeout_period = 0xF000         // Receive response timeout in usec.
};

static ss_twr_frame_t ss_twr[] = {
    [0].request = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID
    },
    [1].request = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID
    }
};

void print_frame(const char * name, ss_twr_frame_t * ss_twr ){
    printf("%s{\n\tfctrl:0x%04X,\n", name, ss_twr->response.fctrl);
    printf("\tseq_num:0x%02X,\n", ss_twr->response.seq_num);
    printf("\tPANID:0x%04X,\n", ss_twr->response.PANID);
    printf("\tdst_address:0x%04X,\n", ss_twr->response.dst_address);
    printf("\tsrc_address:0x%04X,\n", ss_twr->response.src_address);
    printf("\tcode:0x%04X,\n", ss_twr->response.code);
    printf("\treception_timestamp:0x%08lX,\n", ss_twr->response.reception_timestamp); 
    printf("\ttransmission_timestamp:0x%08lX,\n", ss_twr->response.transmission_timestamp); 
    printf("\trequest_timestamp:0x%08lX,\n", ss_twr->request_timestamp); 
    printf("\tresponse_timestamp:0x%08lX\n}\n", ss_twr->response_timestamp); 
    return;
}

/* The timer callout */
static struct os_callout blinky_callout;
/*
 * Event callback function for timer events. 
*/
static void timer_ev_cb(struct os_event *ev) {
    assert(ev != NULL);

    hal_gpio_toggle(LED_BLINK_PIN);
    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
    
    assert(inst->rng->nframes > 0);

#if 0
    if (dw1000_rng_request(inst, NULL, NULL, DWT_SS_TWR).rx_error)
        printf("timer_ev_cb:rng_request failed [status.mac_error]\n");
#else

    if (inst->status.start_rx_error)
        printf("timer_ev_cb:[start_rx_error]\n");
    if (inst->status.start_tx_error)
        printf("timer_ev_cb:[start_tx_error]\n");
    if (inst->status.rx_error)
        printf("timer_ev_cb:[rx_error]\n");
    if (inst->status.request_timeout)
        printf("timer_ev_cb:[request_timeout]\n");
    if (inst->status.rx_timeout_error)
        printf("timer_ev_cb:[rx_timeout_error]\n");


    if (inst->status.start_tx_error || inst->status.rx_error || inst->status.request_timeout ||  inst->status.rx_timeout_error){
        inst->status.start_tx_error = inst->status.rx_error = inst->status.request_timeout = inst->status.rx_timeout_error = 0;
        dw1000_set_rx_timeout(inst, 0);
        dw1000_start_rx(inst); 
    }
#endif

    else if (inst->rng->ss_twr[0].response.code == DWT_SS_TWR_FINAL) {
            ss_twr_frame_t * ss_twr  = &inst->rng->ss_twr[0];  
            print_frame("ss_trw=",ss_twr);
            ss_twr->response.code = DWT_SS_TWR_END;

            int32_t ToF = ((ss_twr->response_timestamp - ss_twr->request_timestamp) 
                -  (ss_twr->response.transmission_timestamp - ss_twr->response.reception_timestamp))/2;

            printf("ToF=%lX, res_req=%lX rec_tra=%lX\n", ToF, (ss_twr->response_timestamp - ss_twr->request_timestamp), (ss_twr->response.transmission_timestamp - ss_twr->response.reception_timestamp));
            dw1000_set_rx_timeout(inst, 0);
            dw1000_start_rx(inst); 
    }

    else if (inst->rng->ss_twr[1].response.code == DWT_SDS_TWR_FINAL) {
        print_frame("1st=",&inst->rng->ss_twr[0]);

        inst->rng->ss_twr[1].response.code = DWT_SDS_TWR_END;
        print_frame("2nd=",&inst->rng->ss_twr[1]);
        
        inst->rng->ss_twr[1].response.code = DWT_SDS_TWR_END;

        dw1000_set_rx_timeout(inst, 0);
        dw1000_start_rx(inst); 
    }
    os_callout_reset(&blinky_callout, OS_TICKS_PER_SEC/128);
}

static void init_timer(void) {
    /*
     * Initialize the callout for a timer event.
     */
    os_callout_init(&blinky_callout, os_eventq_dflt_get(), timer_ev_cb, NULL);
    os_callout_reset(&blinky_callout, OS_TICKS_PER_SEC);
}

int main(int argc, char **argv){
    int rc;

    sysinit();
    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
    dw1000_dev_init(inst, MYNEWT_VAL(DW1000_DEVICE_0_SPI_IDX));
    dw1000_phy_init(inst, &txrf_config);
    
    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);

    init_timer();
    dw1000_softreset(inst);
    inst->PANID = 0xDECA;
    inst->my_short_address = 0x4321;
    dw1000_set_panid(inst,inst->PANID);
    dw1000_mac_init(inst, &mac_config);
    dw1000_rng_init(inst, &rng_config);
    dw1000_rng_set_frames(inst, ss_twr, sizeof(ss_twr)/sizeof(ss_twr_frame_t));

    printf("device_id=%lX\n",inst->device_id);
    printf("PANID=%X\n",inst->PANID);
    printf("DeviceID =%X\n",inst->my_short_address);
    printf("partID =%lX\n",inst->partID);
    printf("lotID =%lX\n",inst->lotID);
    printf("xtal_trim =%X\n",inst->xtal_trim);

    dw1000_set_rx_timeout(inst, 0);
    dw1000_start_rx(inst); 

    while (1) {
        os_eventq_run(os_eventq_dflt_get());   
    }

    assert(0);

    return rc;
}

