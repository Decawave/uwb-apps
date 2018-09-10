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
#include "extension.h"
#include <dw1000/dw1000_dev.h>
#include <dw1000/dw1000_hal.h>
#include <dw1000/dw1000_phy.h>
#include <dw1000/dw1000_mac.h>
#include <dw1000/dw1000_rng.h>
#include <dw1000/dw1000_ftypes.h>

#if MYNEWT_VAL(DW1000_CCP_ENABLED)
#include <dw1000/dw1000_ccp.h>
#endif
#if MYNEWT_VAL(TDMA_ENABLED)
#include <dw1000/dw1000_tdma.h>
#endif
#if MYNEWT_VAL(DW1000_LWIP)
#include <dw1000/dw1000_lwip.h>
#endif
#if MYNEWT_VAL(DW1000_PAN)
#include <dw1000/dw1000_pan.h>
#endif
#if MYNEWT_VAL(N_RANGES_NPLUS_TWO_MSGS)
#include <dw1000_nranges.h>
dw1000_nranges_instance_t nranges_instance;
#endif

static dw1000_rng_config_t rng_config = {
    .tx_holdoff_delay = 0x0600,         // Send Time delay in usec.
    .rx_timeout_period = 0x1fff        // Receive response timeout in usec
};

#if MYNEWT_VAL(DW1000_PAN)
static dw1000_pan_config_t pan_config = {
    .tx_holdoff_delay = 0x0C00,         // Send Time delay in usec.
    .rx_timeout_period = 0x8000         // Receive response timeout in usec.
};
#endif

#define N_FRAMES MYNEWT_VAL(N_NODES)*2

static twr_frame_t twr[N_FRAMES] = {
    [0] = {
        .fctrl = FCNTL_IEEE_N_RANGES_16, // frame control (0x8841 to indicate a data frame using 16-bit addressing).
        .PANID = 0xDECA,                 // PAN ID (0xDECA)
        .code = DWT_TWR_INVALID
    }
};

static void set_default_rng_params(twr_frame_t *frame , uint16_t nframes)
{
    uint16_t i ;
    for(i = 1 ; i<nframes ; i++)
    {
        (frame+i)->fctrl = frame->fctrl;
        (frame+i)->PANID = frame->PANID;
        (frame+i)->code  = frame->code;
    }
}

#define NSLOTS MYNEWT_VAL(TDMA_NSLOTS)
#if MYNEWT_VAL(TDMA_ENABLED)
static uint16_t g_slot[5] = {0,1,2,3,4};//{0,1,126,127};//,4,5,6,7,8,9,10,11,12,13,14,15,18,19,20,21,22,23,24,25,26,27,28,29,30,
       // 31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62};
#endif

static bool timeout_cb(struct _dw1000_dev_instance_t * inst);
static bool error_cb(struct _dw1000_dev_instance_t * inst);

/*! 
 * @fn frame_timer_cb(struct os_event * ev)
 *
 * @brief This function each 
 *
 * input parameters
 * @param inst - struct os_event *  
 *
 * output parameters
 *
 * returns none 
 */
static void 
slot_timer_cb(struct os_event *ev){
    assert(ev);

    tdma_slot_t * slot = (tdma_slot_t *) ev->ev_arg;
    tdma_instance_t * tdma = slot->parent;
    dw1000_dev_instance_t * inst = tdma->parent;
    dw1000_rng_instance_t * rng = inst->rng;
    dw1000_nranges_instance_t * nranges = &nranges_instance;
    uint16_t nnodes = nranges->nnodes;
    nranges->t1_final_flag = 1;

    clkcal_instance_t * clk = inst->ccp->clkcal;
    uint16_t idx = slot->idx;

    hal_gpio_toggle(LED_BLINK_PIN);

#if MYNEWT_VAL(ADAPTIVE_TIMESCALE_ENABLED) 
    uint64_t dx_time = (clk->epoch + (uint64_t) roundf(clk->skew * (double)((idx * (uint64_t)tdma->period << 16)/tdma->nslots)));
#else
    uint64_t dx_time = (clk->epoch + (uint64_t) (idx * ((uint64_t)tdma->period << 16)/tdma->nslots));
#endif
    dx_time = dx_time  & 0xFFFFFFFE00UL;
    //    uint32_t tic = os_cputime_ticks_to_usecs(os_cputime_get32());
    if(dw1000_nranges_request_delay_start(inst, 0xffff, dx_time, DWT_DS_TWR_NRNG).start_tx_error){
        uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
        printf("{\"utime\": %lu,\"msg\": \"slot_timer_cb_%d:start_tx_error\"}\n",utime,idx);
    }else{
        //        uint32_t toc = os_cputime_ticks_to_usecs(os_cputime_get32());
        //        printf("{\"utime\": %lu,\"slot_timer_cb_tic_toc\": %lu}\n",toc,toc-tic);
    }
    twr_frame_t * previous_frame = rng->frames[(rng->idx-1)%rng->nframes];
    twr_frame_t * frame = rng->frames[(rng->idx)%rng->nframes];
    //printf("rng->idx == %d \n",(rng->idx-1)%rng->nframes);

    if (inst->status.start_rx_error)
        printf("{\"utime\": %lu,\"timer_ev_cb\": \"start_rx_error\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()));
    if (inst->status.start_tx_error)
        printf("{\"utime\": %lu,\"timer_ev_cb\":\"start_tx_error\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()));
    if (inst->status.rx_error)
        printf("{\"utime\": %lu,\"timer_ev_cb\":\"rx_error\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()));
    if (inst->status.rx_timeout_error)
        printf("{\"utime\":%lu,\"Rx-TimeOut Error::missing %u responses\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()),nnodes-nranges->resp_count);

    if (inst->status.start_tx_error || inst->status.start_rx_error || inst->status.rx_error
            || inst->status.rx_timeout_error){
        rng->idx = 0xffff;
    }

    if (frame->code == DWT_DS_TWR_NRNG_FINAL || frame->code == DWT_DS_TWR_NRNG_EXT_FINAL) {
        previous_frame = rng->frames[0];
        int i;
        for(i = 0 ; i < nranges->resp_count ; i++)
        {
            float range = dw1000_rng_tof_to_meters(dw1000_nranges_twr_to_tof_frames(previous_frame+i, previous_frame+i+nnodes));
            printf("  src_addr= 0x%X  dst_addr= 0x%X  range= %lu\n",(previous_frame+i)->src_address,(previous_frame+i)->dst_address, (uint32_t)(range*1000));
            (previous_frame+i+nnodes)->code = DWT_DS_TWR_NRNG_END;
            (previous_frame+i)->code = DWT_DS_TWR_NRNG_END;
        }
        printf("time-secs:: %lu\n", os_cputime_ticks_to_usecs(os_cputime_get32())/1000000);
        rng->idx = 0xffff;
        nranges->resp_count = 0;
    }

}

/*! 
 * @fn timeout_cb(struct os_event *ev)
 *
 * @brief This callback is in the interrupt context and is called on timeout event.
 * In this example re enable rx.
 * Note interrupt context so overlapping IO is possible
 * input parameters
 * @param inst - dw1000_dev_instance_t * inst
 *
 * output parameters
 *
 * returns none 
 */
static bool
timeout_cb(struct _dw1000_dev_instance_t * inst) {
    if(inst->fctrl != FCNTL_IEEE_RANGE_16){
        return false;
    }

    if (inst->status.rx_timeout_error){
        printf("{\"utime\": %lu,\"msg\": \"timeout_cb::rx_timeout_error\"}\n",os_cputime_ticks_to_usecs(os_cputime_get32()));
    }

    if (inst->tdma->status.awaiting_superframe){
        dw1000_set_rx_timeout(inst, 0);
        dw1000_start_rx(inst); 
    }
    return true;
}

/*! 
 * @fn error_cb(struct os_event *ev)
 *
 * @brief This callback is in the interrupt context and is called on error event.
 * In this example just log event. 
 * Note: interrupt context so overlapping IO is possible
 * input parameters
 * @param inst - dw1000_dev_instance_t * inst
 *
 * output parameters
 *
 * returns none 
 */
static bool
error_cb(struct _dw1000_dev_instance_t * inst) {
    if(inst->fctrl != FCNTL_IEEE_RANGE_16){
        return false;
    }   
    uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
    if (inst->status.start_rx_error)
        printf("{\"utime\": %lu,\"error_cb\": \"start_rx_error\"}\n",utime);
    if (inst->status.start_tx_error)
        printf("{\"utime\": %lu,\"error_cb\":\"start_tx_error\"}\n",utime);
    if (inst->status.rx_error)
        printf("{\"utime\": %lu,\"error_cb\":\"rx_error\"}\n",utime);

    if (inst->tdma->status.awaiting_superframe){
        printf("{\"utime\": %lu,\"error_cb\":\"awaiting_superframe\"}\n",utime); 
        dw1000_set_rx_timeout(inst, 0);
        dw1000_start_rx(inst); 
    }
    return true;
}

static bool
tx_complete_cb(dw1000_dev_instance_t* inst){
    if(inst->fctrl != FCNTL_IEEE_RANGE_16){
        return false;
    }
    return true;
}


/*! 
 * @fn superres_complete_cb(dw1000_dev_instance_t * inst)
 *
 * @brief This callback is in the interrupt context and is uses to schedule an pdoa_complete event on the default event queue.  
 * Processing should be kept to a minimum giving the context. All algorithms should be deferred to a thread on an event queue. 
 * In this example all postprocessing is performed in the pdoa_ev_cb.
 * input parameters
 * @param inst - dw1000_dev_instance_t * 
 *
 * output parameters
 *
 * returns none 
 */
/* The timer callout */
//static struct os_callout slot_complete_callout;

static bool 
complete_cb(struct _dw1000_dev_instance_t * inst){
    if(inst->fctrl != FCNTL_IEEE_RANGE_16){
        return false;
    }
    //os_callout_init(&slot_complete_callout, os_eventq_dflt_get(), slot_complete_cb, inst);
    //os_eventq_put(os_eventq_dflt_get(), &slot_complete_callout.c_ev);
    
    if (inst->tdma->status.awaiting_superframe){
            uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
            printf("{\"utime\": %lu,\"complete_cb\":\"awaiting_superframe\"}\n",utime); 
            dw1000_set_rx_timeout(inst, 0);
            dw1000_start_rx(inst); 
    }
    return true;
}

static void
pan_postprocess(struct os_event* ev){
    dw1000_dev_instance_t* inst = (dw1000_dev_instance_t*)ev->ev_arg;
    if(inst->pan->status.valid != true){
        os_sem_release(&inst->pan->sem_waitforsucess);
    }else{
        dw1000_set_address16(inst, inst->my_short_address);
        printf("Discovery Completed \n");
        printf("DeviceID =%X\n",inst->my_short_address);
        printf("SlotID =%X\n",inst->slot_id);
        tdma_release_slot(inst->tdma, g_slot[1]);
        tdma_assign_slot(inst->tdma, slot_timer_cb, inst->slot_id, &inst->slot_id);
    }
    dw1000_set_rx_timeout(inst, 0);
    dw1000_start_rx(inst);
}

#define SLOT MYNEWT_VAL(SLOT_ID)
#define ALT_SLOT 0
int main(int argc, char **argv){
    int rc;
    dw1000_extension_callbacks_t tdma_cbs;

    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);
    hal_gpio_init_out(LED_1, 1);
    hal_gpio_init_out(LED_3, 1);

    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
    dw1000_softreset(inst);
    dw1000_phy_init(inst, NULL);

    inst->PANID = 0xDECA;
    inst->my_short_address = MYNEWT_VAL(DEVICE_ID) + ALT_SLOT;
    inst->my_long_address = ((uint64_t) inst->device_id << 32) + inst->partID;

    dw1000_set_panid(inst,inst->PANID);
    dw1000_mac_init(inst, &inst->config);
    dw1000_rng_init(inst, &rng_config, sizeof(twr)/sizeof(twr_frame_t));
    dw1000_rng_set_frames(inst, twr, sizeof(twr)/sizeof(twr_frame_t));
    set_default_rng_params(twr, sizeof(twr)/sizeof(twr_frame_t));

    tdma_cbs.tx_error_cb = error_cb;
    tdma_cbs.rx_error_cb = error_cb;
    tdma_cbs.rx_timeout_cb = timeout_cb;
    tdma_cbs.rx_complete_cb = complete_cb;
    tdma_cbs.tx_complete_cb = tx_complete_cb;
    tdma_cbs.id = DW1000_RANGE;
    dw1000_add_extension_callbacks(inst, tdma_cbs);

#if MYNEWT_VAL(DW1000_CCP_ENABLED)
    dw1000_ccp_init(inst, 2, MYNEWT_VAL(UUID_CCP_MASTER));
#endif
#if MYNEWT_VAL(DW1000_PAN)
    dw1000_extension_callbacks_t pan_cbs;
    dw1000_pan_init(inst, &pan_config);
    dw1000_pan_set_postprocess(inst, pan_postprocess);
    dw1000_remove_extension_callbacks(inst, DW1000_PAN);
    pan_cbs.tx_complete_cb = pan_tx_complete_cb;
    pan_cbs.rx_complete_cb = pan_rx_complete_cb;
    pan_cbs.rx_timeout_cb = pan_rx_timeout_cb;
    pan_cbs.rx_error_cb = pan_rx_error_cb;
    pan_cbs.tx_error_cb = pan_tx_error_cb;
    dw1000_pan_set_ext_callbacks(inst, pan_cbs);
    dw1000_pan_start(inst, DWT_NONBLOCKING);
#endif
#if MYNEWT_VAL(N_RANGES_NPLUS_TWO_MSGS)
    dw1000_nranges_instance_t * nranges = &nranges_instance;
    memset(nranges,0,sizeof(dw1000_nranges_instance_t));
    nranges->initiator = 1;
    nranges->nnodes= MYNEWT_VAL(N_NODES);
    dw1000_nranges_init(inst, nranges);
    printf("number of nodes  ===== %u \n",nranges->nnodes);
#endif
    printf("device_id = 0x%lX\n",inst->device_id);
    printf("PANID = 0x%X\n",inst->PANID);
    printf("DeviceID = 0x%X\n",inst->my_short_address);
    printf("partID = 0x%lX\n",inst->partID);
    printf("lotID = 0x%lX\n",inst->lotID);
    printf("xtal_trim = 0x%X\n",inst->xtal_trim);
    printf("no of frames == %u \n",sizeof(twr)/sizeof(twr_frame_t));

#if MYNEWT_VAL(TDMA_ENABLED) 
   for (uint16_t i = 0; i < sizeof(g_slot)/sizeof(uint16_t); i++)
        g_slot[i] = i;
    tdma_init(inst, MYNEWT_VAL(TDMA_SUPERFRAME_PERIOD), 5);
    tdma_assign_slot(inst->tdma, slot_timer_cb, g_slot[SLOT], &g_slot[SLOT]);
#else
    dw1000_set_rx_timeout(inst, 0);
    dw1000_start_rx(inst); 
#endif

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}
