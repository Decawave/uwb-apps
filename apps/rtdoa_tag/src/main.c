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

#include <uwb/uwb.h>
#include "uwb/uwb_ftypes.h"
#include "dw1000/dw1000_hal.h"
#include "uwbcfg/uwbcfg.h"
#include <config/config.h>

#if MYNEWT_VAL(NMGR_UWB_ENABLED)
#include <nmgr_uwb/nmgr_uwb.h>
#endif

#include <tdma/tdma.h>
#include <uwb_ccp/uwb_ccp.h>
#include <uwb_wcs/uwb_wcs.h>
#include <timescale/timescale.h>
#if MYNEWT_VAL(UWB_RNG_ENABLED)
#include <uwb_rng/uwb_rng.h>
#endif
#include <rtdoa/rtdoa.h>

#include <rtdoa_backhaul/rtdoa_backhaul.h>
#include "sensor/sensor.h"
#include "sensor/accel.h"
#include "sensor/gyro.h"
#include "sensor/mag.h"
#include "sensor/pressure.h"
static struct dpl_callout sensor_callout;
#if MYNEWT_VAL(IMU_RATE)
static int imu_reset_ticks = DPL_TICKS_PER_SEC/MYNEWT_VAL(IMU_RATE);
#endif
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
sensor_timer_ev_cb(struct dpl_event *ev)
{
    assert(ev != NULL);
    static uint32_t last_batt_update = 0;
    static uint32_t last_mp_update = 0;
    const float mv2V = 0.001f;
    int rc;
    struct sensor *s;
    sensor_type_t sensor_types[] = {SENSOR_TYPE_ACCELEROMETER,
                                    SENSOR_TYPE_GYROSCOPE,
                                    SENSOR_TYPE_MAGNETIC_FIELD,
                                    SENSOR_TYPE_PRESSURE,
                                    SENSOR_TYPE_NONE};
    uint64_t local_ts = uwb_read_systime(uwb_dev_idx_lookup(0));

    /* Only include pressure and compass at max 20hz */
    if (os_cputime_get32() - last_mp_update > os_cputime_usecs_to_ticks(50000)) {
        last_mp_update = os_cputime_get32();
    } else {
        sensor_types[2] = SENSOR_TYPE_NONE;
    }

    int i=0;
    while (sensor_types[i] != SENSOR_TYPE_NONE) {
        s = sensor_mgr_find_next_bytype(sensor_types[i], NULL);
        if (s) {
            rc = sensor_read(s, sensor_types[i], &rtdoa_backhaul_sensor_data_cb, 0,
                             DPL_TICKS_PER_SEC/100);
            if (rc) continue;
        }

        i++;
    }

    /* Only include battery information once a second */
    if (os_cputime_get32() - last_batt_update > os_cputime_usecs_to_ticks(1000000)) {
        last_batt_update = os_cputime_get32();

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
#if defined(USB_V_PIN)
        int16_t usb_mv = hal_bsp_read_usb_voltage();
#else
        int16_t usb_mv = -1;
#endif
        if (usb_mv > -1) {
            rtdoa_backhaul_usb_cb(usb_mv*mv2V);
        }

        if (g_battery_voltage < 2.900) {
            if (usb_mv < 3000) {
                low_battery_mode();
            }
        }
    }

    if (rtdoa_backhaul_queue_size()>2) {
        goto early_exit;
    }

    // Translate our timestamp into the UWB network-master's timeframe
    struct uwb_ccp_instance *ccp = (struct uwb_ccp_instance*)uwb_mac_find_cb_inst_ptr(uwb_dev_idx_lookup(0), UWBEXT_CCP);
    if (ccp->status.valid) {
        uint64_t ts = uwb_wcs_local_to_master64(ccp->wcs, local_ts);
        rtdoa_backhaul_send_imu_only(ts>>16);
    } else {
        rtdoa_backhaul_send_imu_only(0);
    }
early_exit:
#if MYNEWT_VAL(IMU_RATE)
    dpl_callout_reset(&sensor_callout, imu_reset_ticks);
#endif
    return;
}

static void
init_timer(void)
{
    dpl_callout_init(&sensor_callout, dpl_eventq_dflt_get(), sensor_timer_ev_cb, NULL);
#if MYNEWT_VAL(IMU_RATE)
    /* Kickoff a new imu reading directly */
    dpl_callout_reset(&sensor_callout, 0);
#endif
}

static bool uwb_config_updated = false;
static int
uwb_config_upd_cb()
{
    /* Workaround in case we're stuck waiting for ccp with the
     * wrong radio settings */
    struct uwb_dev *inst = uwb_dev_idx_lookup(0);
    struct uwb_ccp_instance *ccp = (struct uwb_ccp_instance*)uwb_mac_find_cb_inst_ptr(inst, UWBEXT_CCP);
    if (dpl_sem_get_count(&ccp->sem) == 0 || !ccp->status.valid) {
        uwb_mac_config(inst, NULL);
        uwb_txrf_config(inst, &inst->config.txrf);
        if (dpl_sem_get_count(&ccp->sem) == 0) {
            uwb_start_rx(inst);
        }
        return 0;
    }
    uwb_config_updated = true;
    return 0;
}
struct uwbcfg_cbs uwb_cb = {
    .uc_update = uwb_config_upd_cb
};


/**
 * @fn rtdoa_slot_timer_cb(struct dpl_event * ev)
 *
 * @brief RTDoA Subscription slot
 *
 */
static void
rtdoa_slot_timer_cb(struct dpl_event *ev)
{
    assert(ev);
    tdma_slot_t * slot = (tdma_slot_t *) dpl_event_get_arg(ev);
    tdma_instance_t * tdma = slot->parent;
    struct uwb_ccp_instance * ccp = tdma->ccp;
    struct uwb_dev * inst = tdma->dev_inst;
    uint16_t idx = slot->idx;
    struct rtdoa_instance * rtdoa = (struct rtdoa_instance*)slot->arg;
    //printf("idx%d\n", idx);

    /* Avoid colliding with the ccp */
    if (dpl_sem_get_count(&ccp->sem) == 0) {
        return;
    }
    hal_gpio_write(LED_BLINK_PIN,1);
    uint64_t dx_time = tdma_rx_slot_start(tdma, idx);
    if(rtdoa_listen(rtdoa, UWB_BLOCKING, dx_time, 3*ccp->period/tdma->nslots/4).start_rx_error) {
        printf("#rse\n");
    }
    hal_gpio_write(LED_BLINK_PIN,0);

    if (dpl_sem_get_count(&ccp->sem) == 0) {
        return;
    }
    uint64_t measurement_ts = uwb_wcs_local_to_master64(ccp->wcs, dx_time);
    rtdoa_backhaul_set_ts(measurement_ts>>16);
    rtdoa_backhaul_send(inst, rtdoa, 0); //tdma_tx_slot_start(inst, idx+2)
    //printf("idx%de\n", idx);
}

static void
nmgr_slot_timer_cb(struct dpl_event * ev)
{
    assert(ev);
    tdma_slot_t * slot = (tdma_slot_t *) dpl_event_get_arg(ev);
    tdma_instance_t * tdma = slot->parent;
    struct uwb_ccp_instance *ccp = tdma->ccp;
    struct uwb_dev * inst = tdma->dev_inst;
    uint16_t idx = slot->idx;
    nmgr_uwb_instance_t * nmgruwb = (nmgr_uwb_instance_t *)slot->arg;
    assert(nmgruwb);
    // printf("idx %02d nmgr\n", idx);

    if (uwb_config_updated) {
        uwb_mac_config(inst, NULL);
        uwb_txrf_config(inst, &inst->config.txrf);
        uwb_config_updated = false;
    }

    /* Avoid colliding with the ccp */
    if (dpl_sem_get_count(&ccp->sem) == 0) {
        return;
    }

    if (uwb_nmgr_process_tx_queue(nmgruwb, tdma_tx_slot_start(tdma, idx)) == false) {
        nmgr_uwb_listen(nmgruwb, UWB_BLOCKING, tdma_rx_slot_start(tdma, idx),
             3*ccp->period/tdma->nslots/4);
    }
}


static void
tdma_allocate_slots(tdma_instance_t * tdma)
{
    uint16_t i;
    /* Pan for anchors is in slot 1 */
    struct uwb_dev * inst = tdma->dev_inst;
    nmgr_uwb_instance_t *nmgruwb = (nmgr_uwb_instance_t*)uwb_mac_find_cb_inst_ptr(inst, UWBEXT_NMGR_UWB);
    assert(nmgruwb);
    struct rtdoa_instance * rtdoa = (struct rtdoa_instance*)uwb_mac_find_cb_inst_ptr(inst, UWBEXT_RTDOA);
    assert(rtdoa);

    /* anchor-to-anchor range slot is 31 */
    for (i=2;i < MYNEWT_VAL(TDMA_NSLOTS);i++) {
        if (i==31) {
            continue;
        }
        if (i%12==0) {
            tdma_assign_slot(tdma, nmgr_slot_timer_cb, i, nmgruwb);
        } else {
            tdma_assign_slot(tdma, rtdoa_slot_timer_cb, i, rtdoa);
        }
    }
}

static void
low_battery_mode()
{
    /* Shutdown and sleep dw1000 */
    struct uwb_dev *inst = uwb_dev_idx_lookup(0);
    tdma_instance_t * tdma = (tdma_instance_t*)uwb_mac_find_cb_inst_ptr(inst, UWBEXT_TDMA);
    tdma_stop(tdma);
    uwb_ccp_stop(tdma->ccp);
    hal_gpio_irq_disable(hal_dw1000_inst(0)->irq_pin);
    inst->config.rxauto_enable = 0;
    uwb_phy_forcetrxoff(inst);
    uwb_mac_config(inst, NULL);

    /* Force timeout */
    uwb_set_rx_timeout(inst, 1);
    uwb_phy_forcetrxoff(inst);

    dpl_time_delay(DPL_TICKS_PER_SEC/10);
    uwb_sleep_config(inst);
    uwb_enter_sleep(inst);

#if defined(BATT_V_PIN)
    int16_t batt_mv = hal_bsp_read_battery_voltage();
#else
    int16_t batt_mv = -1;
#endif
    int16_t usb_mv = 0;//hal_bsp_read_usb_voltage();
    while (batt_mv < 3000 && usb_mv < 3000) {
        printf("{\"battery_low\"=%d,\"usb\"=%d}\n", batt_mv, usb_mv);
        for (int i=0;i<3;i++) {
            hal_gpio_write(LED_BLINK_PIN,1);
            dpl_time_delay(1);
            hal_gpio_write(LED_BLINK_PIN,0);
            dpl_time_delay(DPL_TICKS_PER_SEC/10);
        }
        for (int i=0;i<50;i++) {
            /* Consume any events */
            struct dpl_event *ev;
            ev = dpl_eventq_get_no_wait(dpl_eventq_dflt_get());
            if (ev != NULL) {
                dpl_event_run(ev);
            }
            dpl_time_delay(DPL_TICKS_PER_SEC/10);
        }
#if defined(BATT_V_PIN)
        batt_mv = hal_bsp_read_battery_voltage();
#else
        batt_mv = -1;
#endif
        usb_mv = 0;//hal_bsp_read_usb_voltage();
    }
    printf("{\"rebooting at mv\"=%d}\n", batt_mv);
    dpl_time_delay(DPL_TICKS_PER_SEC);
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

    struct uwb_dev *udev = uwb_dev_idx_lookup(0);

    udev->config.rxauto_enable = 1;
    udev->config.trxoff_enable = 1;
    udev->config.rxdiag_enable = 1;
    udev->config.sleep_enable = 1;
    udev->config.dblbuffon_enabled = 0;
    uwb_set_dblrxbuff(udev, false);

    udev->slot_id = 0;

    ble_init(udev->my_long_address);

    printf("{\"device_id\"=\"%lX\"",udev->device_id);
    printf(",\"panid=\"%X\"",udev->pan_id);
    printf(",\"addr\"=\"%X\"",udev->uid);
    printf(",\"part_id\"=\"%lX\"",(uint32_t)(udev->euid&0xffffffff));
    printf(",\"lot_id\"=\"%lX\"}\n",(uint32_t)(udev->euid>>32));

    struct uwb_ccp_instance * ccp = (struct uwb_ccp_instance*)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_CCP);
    assert(ccp);
    tdma_instance_t * tdma = (tdma_instance_t*)uwb_mac_find_cb_inst_ptr(udev, UWBEXT_TDMA);
    assert(tdma);

    tdma_allocate_slots(tdma);
    uwb_ccp_start(ccp, CCP_ROLE_SLAVE);
    rtdoa_backhaul_set_role(udev, RTDOABH_ROLE_BRIDGE);

    init_timer();

#if MYNEWT_VAL(NCBWIFI_ESP_PASSTHROUGH)
    hal_bsp_esp_bypass(true);
#endif

    while (1) {
        dpl_eventq_run(dpl_eventq_dflt_get());
    }
    assert(0);
    return rc;
}
