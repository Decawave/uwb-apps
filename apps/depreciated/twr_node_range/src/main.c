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
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif

#include <dw1000/dw1000_dev.h>
#include <dw1000/dw1000_hal.h>
#include <dw1000/dw1000_phy.h>
#include <dw1000/dw1000_mac.h>
#include <dw1000/dw1000_rng.h>
#include <dw1000/dw1000_ftypes.h>
#include <json_encode.h>

#if MYNEWT_VAL(DW1000_CCP_ENABLED)
#include <dw1000/dw1000_ccp.h>
#endif
#if MYNEWT_VAL(DW1000_LWIP)
#include <dw1000/dw1000_lwip.h>
#endif
#if MYNEWT_VAL(DW1000_PAN)
#include <dw1000/dw1000_pan.h>
#endif
#if MYNEWT_VAL(DW1000_RANGE)
#include <range/dw1000_range.h>
#endif


static twr_frame_t twr[] = {
    [0] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                 // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID,
    },
    [1] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                 // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID,
    },
    [2] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                 // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID,
    },
    [3] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                 // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID,
    }
};
static twr_frame_t twr_rev[] = {
    [0] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                 // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID
    },
    [1] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                 // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID,
    },
    [2] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                 // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID
    },
    [3] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                 // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID,
    },
    [4] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                 // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID
    },
    [5] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                 // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID,
    },
    [6] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                 // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID
    },
    [7] = {
        .fctrl = 0x8841,                // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                 // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID,
    }                                                                                                                   
};

static dw1000_rng_config_t rng_config = {
    .tx_holdoff_delay = 0x0380,         // Send Time delay in usec.
    .rx_timeout_period = 0x0         // Receive response timeout in usec
};

#if MYNEWT_VAL(DW1000_PAN)
static dw1000_pan_config_t pan_config = {
    .tx_holdoff_delay = 0x0C00,         // Send Time delay in usec.
    .rx_timeout_period = 0x8000         // Receive response timeout in usec.
};
#endif


#if MYNEWT_VAL(DW1000_RANGE)
static uint16_t node_addr[] = {0xDEC1, 0xDEC2};
static uint16_t node_addr_rev[] = {0xDEC1, 0xDEC2, 0xDEC3, 0xDEC4};
static uint32_t cntr = 0;
#endif


void print_frame(const char * name, twr_frame_t *twr ){
    printf("%s{\n\tfctrl:0x%04X,\n", name, twr->fctrl);
    printf("\tseq_num:0x%02X,\n", twr->seq_num);
    printf("\tPANID:0x%04X,\n", twr->PANID);
    printf("\tdst_address:0x%04X,\n", twr->dst_address);
    printf("\tsrc_address:0x%04X,\n", twr->src_address);
    printf("\tcode:0x%04X,\n", twr->code);
    printf("\treception_timestamp:0x%08lX,\n", twr->reception_timestamp);
    printf("\ttransmission_timestamp:0x%08lX,\n", twr->transmission_timestamp);
    printf("\trequest_timestamp:0x%08lX,\n", twr->request_timestamp);
    printf("\tresponse_timestamp:0x%08lX\n}\n", twr->response_timestamp);
}

#if MYNEWT_VAL(DW1000_RANGE_NODE_JSON)
static void range_postprocess(struct os_event * ev){
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);

    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *)ev->ev_arg;
    dw1000_range_instance_t * range = inst->range;
    dw1000_rng_instance_t * rng = inst->rng;
    twr_frame_t *previous_frame;
    twr_frame_t *frame;
    uint32_t time_of_flight;
    float dist;

    for(uint16_t i = 0; i < range->pp_idx_cnt ; i++){
        frame = rng->frames[(range->pp_idx_list[i])%rng->nframes];
        if (frame->code == DWT_SS_TWR_FINAL) {
            time_of_flight = (uint32_t) dw1000_rng_twr_to_tof(frame, frame);
            json_rng_encode(frame, 1);
            printf("{\"utime\": %lu,\"tof\": %lu,\"range\": %lu}\n",
                    os_cputime_ticks_to_usecs(os_cputime_get32()),
                    time_of_flight,
                    *(uint32_t *)&range
                );
            frame->code = DWT_SS_TWR_END;
        }
        if(range->pp_idx_list[i] == 0)
            previous_frame = rng->frames[(rng->nframes - 1)%rng->nframes];
        else
            previous_frame = rng->frames[(range->pp_idx_list[i] - 1)%rng->nframes];
        frame = rng->frames[(range->pp_idx_list[i])%rng->nframes];
        if ((frame->code == DWT_DS_TWR_FINAL && previous_frame->code == DWT_DS_TWR_T1) || frame->code == DWT_DS_TWR_EXT_FINAL) {
                json_rng_encode(previous_frame, 2);
                uint32_t time_of_flight = (uint32_t) dw1000_rng_twr_to_tof(previous_frame, frame);
                dist = dw1000_rng_tof_to_meters(dw1000_rng_twr_to_tof(previous_frame, frame));
                cir_t cir;
                cir.fp_idx = dw1000_read_reg(inst, RX_TIME_ID, RX_TIME_FP_INDEX_OFFSET, sizeof(uint16_t));
                cir.fp_idx = roundf(((float) (cir.fp_idx >> 6) + 0.5f)) - 2;
                dw1000_read_accdata(inst, (uint8_t *)&cir,  cir.fp_idx * sizeof(cir_complex_t), CIR_SIZE * sizeof(cir_complex_t) + 1);
                json_cir_encode(&cir, "cir", CIR_SIZE);

                if(inst->config.rxdiag_enable)
                json_rxdiag_encode(&inst->rxdiag, "rxdiag");
                printf("{\"utime\": %lu,\"tof\": %lu,\"range\": %lu}\n",
                        os_cputime_ticks_to_usecs(os_cputime_get32()),
                        time_of_flight,
                        *(uint32_t *)&dist
                    );
               frame->code = DWT_DS_TWR_END;
               previous_frame->code = DWT_DS_TWR_END;
        }
    }
}
#else
static void range_postprocess(struct os_event * ev){
    assert(ev != NULL);
    assert(ev->ev_arg != NULL);

    dw1000_dev_instance_t * inst = (dw1000_dev_instance_t *)ev->ev_arg;
    dw1000_range_instance_t * range = inst->range;
    dw1000_rng_instance_t * rng = inst->rng;

    for(uint16_t i = 0; i < range->pp_idx_cnt ; i++){
        twr_frame_t *previous_frame;
        twr_frame_t *frame;
        uint32_t time_of_flight;
        float dist;
        frame = rng->frames[(range->pp_idx_list[i])%rng->nframes];
        if (frame->code == DWT_SS_TWR_FINAL) {
            time_of_flight = (uint32_t) dw1000_rng_twr_to_tof(frame, frame);
            dist = dw1000_rng_tof_to_meters(dw1000_rng_twr_to_tof(frame, frame));
            float rssi = dw1000_get_rssi(inst);
            printf("\tseq_num:0x%02X,\n", frame->seq_num);
            printf("\tPANID:0x%04X,\n", frame->PANID);
            printf("\tdst_address:0x%04X,\n", frame->dst_address);
            printf("\tsrc_address:0x%04X,\n", frame->src_address);
            printf("{\"utime\": %lu,\"tof\": %lu,\"range\": %lu,\"res_req\": %lX,"
                    " \"rec_tra\": %lX, \"rssi\": %d}\n",
                    os_cputime_ticks_to_usecs(os_cputime_get32()),
                    time_of_flight,
                    (uint32_t)(dist * 1000),
                    (frame->response_timestamp - frame->request_timestamp),
                    (frame->transmission_timestamp - frame->reception_timestamp),
                    (int)(rssi)
                  );
            frame->code = DWT_SS_TWR_END;
        }

        if(range->pp_idx_list[i] == 0)
            previous_frame = rng->frames[(rng->nframes - 1)%rng->nframes];
        else
            previous_frame = rng->frames[(range->pp_idx_list[i] - 1)%rng->nframes];
        frame = rng->frames[(range->pp_idx_list[i])%rng->nframes];
        if ((frame->code == DWT_DS_TWR_FINAL && previous_frame->code == DWT_DS_TWR_T1) || frame->code == DWT_DS_TWR_EXT_FINAL) {
            time_of_flight = (uint32_t) dw1000_rng_twr_to_tof(previous_frame, frame);
            dist = dw1000_rng_tof_to_meters(dw1000_rng_twr_to_tof(previous_frame, frame));
            float rssi = dw1000_get_rssi(inst);
            printf("\tseq_num:0x%02X,\n", frame->seq_num);
            printf("\tPANID:0x%04X,\n", frame->PANID);
            printf("\tdst_address:0x%04X,\n", frame->dst_address);
            printf("\tsrc_address:0x%04X,\n", frame->src_address);

            printf("{\"utime\": %lu,\"tof\": %lu,\"range\": %lu,\"res_req\": %lX,"
                    " \"rec_tra\": %lX, \"rssi\": %d}\n",
                    os_cputime_ticks_to_usecs(os_cputime_get32()),
                    time_of_flight,
                    (uint32_t)(dist * 1000),
                    (frame->response_timestamp - frame->request_timestamp),
                    (frame->transmission_timestamp - frame->reception_timestamp),
                    (int)(rssi)
                  );
            frame->code = DWT_DS_TWR_END;
            previous_frame->code = DWT_DS_TWR_END;
        }
    }
    //Dynamically increase the number of nodes, frames        
    if(cntr++ == 100){
        dw1000_range_stop(inst);
        dw1000_rng_reset_frames(inst, twr_rev, sizeof(twr_rev)/sizeof(twr_frame_t));
        dw1000_range_reset_nodes(inst, node_addr_rev, 4);
        dw1000_range_start(inst, DWT_DS_TWR);
    }
}
#endif


int main(int argc, char **argv){
    int rc;
    
    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);
    
    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);

    inst->PANID = 0xDECA;
    inst->my_short_address = MYNEWT_VAL(DW_DEVICE_ID_0);
    inst->my_long_address = ((uint64_t) inst->device_id << 32) + inst->partID;

    dw1000_set_panid(inst,inst->PANID);
    dw1000_set_address16(inst, inst->my_short_address);
    dw1000_mac_framefilter(inst, DWT_FF_DATA_EN|DWT_FF_RSVD_EN );

    dw1000_mac_init(inst, NULL);
    dw1000_rng_init(inst, &rng_config, sizeof(twr)/sizeof(twr_frame_t));
    dw1000_rng_set_frames(inst, twr, sizeof(twr)/sizeof(twr_frame_t));
#if MYNEWT_VAL(DW1000_CCP_ENABLED)
    dw1000_ccp_init(inst, 2, MYNEWT_VAL(UUID_CCP_MASTER));
#endif
#if MYNEWT_VAL(DW1000_PAN)
    dw1000_pan_init(inst, &pan_config);
    dw1000_pan_start(inst, DWT_NONBLOCKING);
    while(inst->pan->status.valid != true){
        os_eventq_run(os_eventq_dflt_get());
        os_cputime_delay_usecs(5000);
    }
#endif
#if MYNEWT_VAL(DW1000_RANGE)
    dw1000_range_init(inst, 2, node_addr);
    dw1000_range_set_postprocess(inst, &range_postprocess);
#endif
    dw1000_set_address16(inst, inst->my_short_address);
    dw1000_mac_framefilter(inst, DWT_FF_DATA_EN);
#if MYNEWT_VAL(DW1000_RANGE)
    dw1000_range_start(inst, DWT_DS_TWR);
#endif
    printf("device_id = 0x%lX\n",inst->device_id);
    printf("PANID = 0x%X\n",inst->PANID);
    printf("DeviceID = 0x%X\n",inst->my_short_address);
    printf("partID = 0x%lX\n",inst->partID);
    printf("lotID = 0x%lX\n",inst->lotID);
    printf("xtal_trim = 0x%X\n",inst->xtal_trim);
  
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}
