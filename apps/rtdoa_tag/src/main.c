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
#include "imgmgr/imgmgr.h"
#include "hal/hal_gpio.h"
#include "hal/hal_bsp.h"
#include <hal/hal_system.h>
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif

#if MYNEWT_VAL(BLEPRPH_ENABLED)
#include "bleprph/bleprph.h"
#endif

#include "dw1000/dw1000_dev.h"
#include "dw1000/dw1000_hal.h"
#include "dw1000/dw1000_phy.h"
#include "dw1000/dw1000_mac.h"
#include "dw1000/dw1000_ftypes.h"
#include "uwbcfg/uwbcfg.h"
#include <config/config.h>

#if MYNEWT_VAL(NMGR_UWB_ENABLED)
#include <nmgr_uwb/nmgr_uwb.h> 
#endif

#include <tdma/tdma.h>
#include <ccp/ccp.h>
#include <wcs/wcs.h>
#include <timescale/timescale.h> 
#if MYNEWT_VAL(RNG_ENABLED)
#include <rng/rng.h>
#endif
#include <rtdoa/rtdoa.h>

#include <rtdoa_backhaul/rtdoa_backhaul.h>
#include "sensor/sensor.h"
#include "sensor/accel.h"
#include "sensor/gyro.h"
#include "sensor/mag.h"
#include "sensor/pressure.h"
static struct os_callout sensor_callout;
static float g_battery_voltage = 5.1;
static void low_battery_mode();

//#define DIAGMSG(s,u) printf(s,u)
#ifndef DIAGMSG
#define DIAGMSG(s,u)
#endif

/**
 * @fn Event callback function for sensor events
*/
static void
sensor_timer_ev_cb(struct os_event *ev) {
    const float mv2V = 0.001f;
    int rc;
    struct sensor *s;
    sensor_type_t sensor_types[] = {SENSOR_TYPE_ACCELEROMETER,
                                    SENSOR_TYPE_GYROSCOPE,
                                    SENSOR_TYPE_MAGNETIC_FIELD,
                                    SENSOR_TYPE_PRESSURE,
                                    SENSOR_TYPE_NONE};
    assert(ev != NULL);
    int i=0;
    while (sensor_types[i] != SENSOR_TYPE_NONE)
    {
        s = sensor_mgr_find_next_bytype(sensor_types[i], NULL);
        if (s)
        {
            rc = sensor_read(s, sensor_types[i], &rtdoa_backhaul_sensor_data_cb, 0,
                             OS_TICKS_PER_SEC/100);
            if (rc) printf("Error: failed to read sensor\n");
        }

        i++;
    }
    
#if defined(BATT_V_PIN)
    int16_t batt_mv = hal_bsp_read_battery_voltage();
#else
    int16_t batt_mv = -1;
#endif
    if (batt_mv > -1) {
        float bvf = 0.10;
        if (g_battery_voltage > 5.0) {
            g_battery_voltage = batt_mv*mv2V;
        } else {
            g_battery_voltage = g_battery_voltage*(1.0-bvf) + bvf*batt_mv*mv2V;
        }
        rtdoa_backhaul_battery_cb(g_battery_voltage);
    }

    //int16_t usb_mv = hal_bsp_read_usb_voltage();
    int16_t usb_mv = 0;
    if (usb_mv > -1) {
        rtdoa_backhaul_usb_cb(usb_mv*mv2V);
    }

    if (g_battery_voltage < 2.900) {
        if (usb_mv < 3000) {
            low_battery_mode();
        }
    }
}

static void
init_timer(void)
{
    os_callout_init(&sensor_callout, os_eventq_dflt_get(), sensor_timer_ev_cb, NULL);
    /* Kickoff a new imu reading directly */
    os_callout_reset(&sensor_callout, 0);
}

static bool dw1000_config_updated = false;
int
uwb_config_updated()
{
    /* Workaround in case we're stuck waiting for ccp with the 
     * wrong radio settings */
    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
    dw1000_ccp_instance_t *ccp = (dw1000_ccp_instance_t*)dw1000_mac_find_cb_inst_ptr(inst, DW1000_CCP);
    if (os_sem_get_count(&ccp->sem) == 0 || !ccp->status.valid) {
        dw1000_mac_config(inst, NULL);
        dw1000_phy_config_txrf(inst, &inst->config.txrf);
        if (os_sem_get_count(&ccp->sem) == 0) {
            dw1000_start_rx(inst);
        }
        return 0;
    }
    dw1000_config_updated = true;
    return 0;
}
struct uwbcfg_cbs uwb_cb = {
    .uc_update = uwb_config_updated
};


/**
 * @fn rtdoa_slot_timer_cb(struct os_event * ev)
 *
 * @brief RTDoA Subscription slot
 *
 */
static void 
rtdoa_slot_timer_cb(struct os_event *ev)
{
    assert(ev);
    tdma_slot_t * slot = (tdma_slot_t *) ev->ev_arg;
    tdma_instance_t * tdma = slot->parent;
    dw1000_ccp_instance_t *ccp = tdma->ccp;
    dw1000_dev_instance_t * inst = tdma->parent;
    uint16_t idx = slot->idx;

    /* Avoid colliding with the ccp */
    if (os_sem_get_count(&ccp->sem) == 0) {
        return;
    }

    hal_gpio_write(LED_BLINK_PIN,1);
    uint64_t dx_time = tdma_rx_slot_start(tdma, idx);
    dw1000_rtdoa_instance_t * rtdoa = (dw1000_rtdoa_instance_t*)dw1000_mac_find_cb_inst_ptr(inst, DW1000_RTDOA);
    if(dw1000_rtdoa_listen(rtdoa, DWT_BLOCKING, dx_time, 3*ccp->period/tdma->nslots/4).start_rx_error) {
        printf("#rse\n");
    }
    hal_gpio_write(LED_BLINK_PIN,0);

    if (os_sem_get_count(&ccp->sem) == 0) {
        return;
    }
    uint64_t measurement_ts = wcs_local_to_master64(ccp->wcs, dx_time);
    rtdoa_backhaul_set_ts(measurement_ts>>16);
    rtdoa_backhaul_send(inst, rtdoa, 0); //tdma_tx_slot_start(inst, idx+2)
    os_callout_reset(&sensor_callout, 0);
}

static void 
nmgr_slot_timer_cb(struct os_event * ev)
{
    assert(ev);
    tdma_slot_t * slot = (tdma_slot_t *) ev->ev_arg;
    tdma_instance_t * tdma = slot->parent;
    dw1000_ccp_instance_t *ccp = tdma->ccp;
    dw1000_dev_instance_t * inst = tdma->parent;
    uint16_t idx = slot->idx;
    // printf("idx %02d nmgr\n", idx);

    if (dw1000_config_updated) {
        dw1000_mac_config(inst, NULL);
        dw1000_phy_config_txrf(inst, &inst->config.txrf);
        dw1000_config_updated = false;
    }

    /* Avoid colliding with the ccp */
    if (os_sem_get_count(&ccp->sem) == 0) {
        return;
    }

    if (uwb_nmgr_process_tx_queue(inst, tdma_tx_slot_start(tdma, idx)) == false) {
        nmgr_uwb_listen(inst, DWT_BLOCKING, tdma_rx_slot_start(tdma, idx),
             3*ccp->period/tdma->nslots/4);
    }
}


static void
tdma_allocate_slots(dw1000_dev_instance_t * inst)
{
    uint16_t i;
    /* Pan is slot 1,2 */
    tdma_instance_t * tdma = (tdma_instance_t*)dw1000_mac_find_cb_inst_ptr(inst, DW1000_TDMA);
    assert(tdma);
    /* anchor-to-anchor range slot is 31 */
    // tdma_assign_slot(inst->tdma, nrng_slot_timer_cb, 31, NULL);
    for (i=3;i < MYNEWT_VAL(TDMA_NSLOTS);i++) {
        if (i==31) {
            continue;
        }
        if (i%12==0) {
            tdma_assign_slot(tdma, nmgr_slot_timer_cb, i, NULL);
            /* TODO: REMOVE below! */
        } else if (i%2==0){
            tdma_assign_slot(tdma, rtdoa_slot_timer_cb, i, NULL);
        }
    }
}

static void
low_battery_mode()
{
    /* Shutdown and sleep dw1000 */
    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);
    tdma_instance_t * tdma = (tdma_instance_t*)dw1000_mac_find_cb_inst_ptr(inst, DW1000_TDMA);
    tdma_stop(tdma);
    dw1000_ccp_stop(inst);
    hal_gpio_irq_disable(inst->irq_pin);
    inst->config.rxauto_enable = 0;
    dw1000_phy_forcetrxoff(inst);
    dw1000_mac_config(inst, NULL);

    /* Force timeout */
    dw1000_set_rx_timeout(inst, 1);
    dw1000_phy_forcetrxoff(inst);

    os_time_delay(OS_TICKS_PER_SEC/10);
    dw1000_dev_configure_sleep(inst);
    dw1000_dev_enter_sleep(inst);

    int16_t batt_mv = 0;//hal_bsp_read_battery_voltage();
    int16_t usb_mv = 0;//hal_bsp_read_usb_voltage();
    while (batt_mv < 3000 && usb_mv < 3000) {
        printf("{\"battery_low\"=%d,\"usb\"=%d}\n", batt_mv, usb_mv);
        for (int i=0;i<3;i++) {
            hal_gpio_write(LED_BLINK_PIN,1);
            os_time_delay(1);
            hal_gpio_write(LED_BLINK_PIN,0);
            os_time_delay(OS_TICKS_PER_SEC/10);
        }
        for (int i=0;i<50;i++) {
            /* Consume any events */
            struct os_event *ev;
            ev = os_eventq_get_no_wait(os_eventq_dflt_get());
            if (ev != NULL) {
                ev->ev_cb(ev);
            }
            os_time_delay(OS_TICKS_PER_SEC/10);
        }
        batt_mv = 0;//hal_bsp_read_battery_voltage(); 
        usb_mv = 0;//hal_bsp_read_usb_voltage();
    }
    printf("{\"rebooting at mv\"=%d}\n", batt_mv);
    os_time_delay(OS_TICKS_PER_SEC);
    hal_system_reset();
}

int
main(int argc, char **argv)
{
    int rc = 0;

    sysinit();
    hal_gpio_init_out(LED_BLINK_PIN, 1);

    uwbcfg_register(&uwb_cb);
    conf_load();

    dw1000_dev_instance_t * inst = hal_dw1000_inst(0);

    inst->config.rxauto_enable = 0;
    inst->config.trxoff_enable = 1;
    inst->config.rxdiag_enable = 1;
    inst->config.sleep_enable = 1;
    inst->config.dblbuffon_enabled = 0;
    dw1000_set_dblrxbuff(inst, false);

    inst->slot_id = 0;
    inst->my_short_address = inst->partID&0xffff;
    inst->my_long_address = ((uint64_t) inst->lotID << 32) + inst->partID;
    dw1000_set_address16(inst,inst->my_short_address);

    ble_init(inst->my_long_address);

    printf("{\"device_id\"=\"%lX\"",inst->device_id);
    printf(",\"PANID=\"%X\"",inst->PANID);
    printf(",\"addr\"=\"%X\"",inst->my_short_address);
    printf(",\"partID\"=\"%lX\"",inst->partID);
    printf(",\"lotID\"=\"%lX\"",inst->lotID);
    printf(",\"slot_id\"=\"%d\"",inst->slot_id);
    printf(",\"xtal_trim\"=\"%X\"}\n",inst->xtal_trim);

    tdma_allocate_slots(inst);
    dw1000_ccp_start(inst, CCP_ROLE_SLAVE);
    rtdoa_backhaul_set_role(inst, RTDOABH_ROLE_BRIDGE);

    init_timer();

#if MYNEWT_VAL(NCBWIFI_ESP_PASSTHROUGH)
    hal_bsp_esp_bypass(true);
#endif

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}

