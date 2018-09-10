/**
 * Copyright 2018, Decawave Limited, All Rights Reserved
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
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <os/os.h>
#include <hal/hal_spi.h>
#include <hal/hal_gpio.h>
#include "bsp/bsp.h"

#include <dw1000/dw1000_regs.h>
#include <dw1000/dw1000_dev.h>
#include <dw1000/dw1000_hal.h>
#include <dw1000/dw1000_mac.h>
#include <dw1000/dw1000_phy.h>
#include <dw1000/dw1000_ftypes.h>
#include <dw1000/dw1000_rng.h>

#if MYNEWT_VAL(DW1000_PROVISION)
#include <dw1000/dw1000_provision.h>
#endif

#include <dw1000/dw1000_pan.h>

/*! 
 * @fn pan_rx_complete_cb(dw1000_dev_instance_t * inst)
 *
 * @brief This is an internal static function that executes on both the pan_master Node and the TAG/ANCHOR 
 * that initiated the blink. On the pan_master the postprecess function should allocate a PANID and a SLOTID, 
 * while on the TAG/ANCHOR the returned allocations are assigned and the PAN discover event is stopped. The pan 
 * discovery resources can be released. 
 *
 * input parameters
 * @param inst - dw1000_dev_instance_t * 
 *
 * output parameters
 *
 * returns none
 */
bool
pan_rx_complete_cb(dw1000_dev_instance_t * inst){
     if(inst->fctrl_array[0] != FCNTL_IEEE_BLINK_TAG_64){
        return false;
    }else if(inst->pan->status.valid == true){
        dw1000_dev_control_t control = inst->control_rx_context;
        dw1000_restart_rx(inst, control);
        return true;
    }
    dw1000_pan_instance_t * pan = inst->pan;
    pan_frame_t * frame = pan->frames[(pan->idx)%pan->nframes];
    if(inst->frame_len == sizeof(struct _pan_frame_resp_t)) {
        dw1000_read_rx(inst, frame->array, 0, sizeof(struct _pan_frame_resp_t));
        if(frame->long_address == inst->my_long_address){
            // TAG/ANCHOR side

            inst->my_short_address = frame->short_address;
            inst->PANID = frame->pan_id;
            inst->slot_id = frame->slot_id;
            pan->status.valid = true;
            dw1000_pan_stop(inst);
            os_sem_release(&pan->sem);
            os_sem_release(&pan->sem_waitforsucess);
        }
    }
    // both pan_master and TAG/ANCHOR
    if (pan->control.postprocess)
        os_eventq_put(os_eventq_dflt_get(), &pan->pan_callout_postprocess.c_ev);
    return true;
}

/*! 
 * @fn pan_tx_complete_cb(dw1000_dev_instance_t * inst)
 *
 * @brief This is an internal static function that executes on the TAG/ANCHOR.
 *
 * input parameters
 * @param inst - dw1000_dev_instance_t * 
 *
 * output parameters
 *
 * returns none
 */
bool
pan_tx_complete_cb(dw1000_dev_instance_t * inst){
   if(inst->fctrl_array[0] != FCNTL_IEEE_BLINK_TAG_64){
        return false;
   }
   dw1000_pan_instance_t * pan = inst->pan;
   os_sem_release(&inst->pan->sem);
   pan->idx++;
    return true;
}

/*!
 * @fn pan_tx_error_cb(dw1000_dev_instance_t * inst)
 *
 * @brief This is an internal static function that executes on the TAG/ANCHOR.
 *
 * input parameters
 * @param inst - dw1000_dev_instance_t *
 *
 * output parameters
 *
 *
 * returns none
 */

bool
pan_tx_error_cb(dw1000_dev_instance_t * inst){
    /* Place holder */
    if(inst->fctrl_array[0] != FCNTL_IEEE_BLINK_TAG_64){
        return false;
    }
    return true;
}

/*!
 * @fn pan_rx_error_cb(dw1000_dev_instance_t * inst)
 *
 * @brief This is an internal static function that executes on the TAG/ANCHOR.
 *
 * input parameters
 * @param inst - dw1000_dev_instance_t *
 *
 * output parameters
 *
 *
 * returns none
 */
bool
pan_rx_error_cb(dw1000_dev_instance_t * inst){
    /* Place holder */
    if(inst->fctrl_array[0] != FCNTL_IEEE_BLINK_TAG_64){
        return false;
    }
    os_sem_release(&inst->pan->sem);
    return true;
}

/*! 
 * @fn pan_rx_timeout_cb(dw1000_dev_instance_t * inst)
 *
 * @brief This is an internal static function that executes on the TAG/ANCHOR.
 *
 * input parameters
 * @param inst - dw1000_dev_instance_t * 
 *
 * output parameters
 *
 * returns none
 */
bool
pan_rx_timeout_cb(dw1000_dev_instance_t * inst){
    //printf("pan_rx_timeout_cb\n");  
    if(inst->fctrl_array[0] != FCNTL_IEEE_BLINK_TAG_64){
        return false;
    }
    dw1000_pan_instance_t * pan = inst->pan;
    if (pan->control.postprocess){
        os_eventq_put(os_eventq_dflt_get(), &pan->pan_callout_postprocess.c_ev);
    }
    return true;
}
