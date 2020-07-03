/*
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
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "console/console.h"
#include "hal/hal_system.h"
#include "config/config.h"
#include "log/log.h"

/* BLE */
#include "bleprph/bleprph.h"
#include "ble_priv.h"
#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

#if MYNEWT_VAL(BLESHELL_MAX_INPUT)
#include "bleshell/bleshell.h"
#endif

struct log bleprph_log;

struct ble_hs_cfg;
struct ble_gatt_register_ctxt;

/* bleprph uses the first "peruser" log module. */
#define BLEPRPH_LOG_MODULE  (LOG_MODULE_PERUSER + 0)

/** GATT server. */
#define GATT_SVR_SVC_ALERT_UUID               0x1811
#define GATT_SVR_CHR_SUP_NEW_ALERT_CAT_UUID   0x2A47
#define GATT_SVR_CHR_NEW_ALERT                0x2A46
#define GATT_SVR_CHR_SUP_UNR_ALERT_CAT_UUID   0x2A48
#define GATT_SVR_CHR_UNR_ALERT_STAT_UUID      0x2A45
#define GATT_SVR_CHR_ALERT_NOT_CTRL_PT        0x2A44

static int bleprph_gap_event(struct ble_gap_event *event, void *arg);
static uint8_t g_bleprph_connected = 0;

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
static void
bleprph_advertise(void)
{
    uint8_t own_addr_type;
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    const char *name;
    int rc;

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        BLEPRPH_LOG(ERROR, "error det addrt;%d\n", rc);
        return;
    }

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     */
    memset(&fields, 0, sizeof fields);

    /* Advertise two flags:
     *     o Discoverability in forthcoming advertisement (general)
     *     o BLE-only (BR/EDR unsupported).
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assiging the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

#if MYNEWT_VAL(BLESHELL_MAX_INPUT)
    fields.uuids128 = BLE_UUID128(&gatt_svr_svc_shell_uuid.u);
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
#endif

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        BLEPRPH_LOG(ERROR, "error set advd;%d\n", rc);
        return;
    }

    memset(&fields, 0, sizeof fields);
    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    fields.uuids16 = (ble_uuid16_t[]){
         BLE_UUID16_INIT(GATT_SVR_SVC_ALERT_UUID)
    };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        BLEPRPH_LOG(ERROR, "error set advd;%d\n", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, bleprph_gap_event, NULL);
    if (rc != 0) {
        BLEPRPH_LOG(ERROR, "error en adv;%d\n", rc);
        return;
    }
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * bleprph uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unuesd by
 *                                  bleprph.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
bleprph_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);

        }

        if (event->connect.status != 0) {
            /* Connection failed; resume advertising. */
            bleprph_advertise();
        } else {
            g_bleprph_connected=1;
#if MYNEWT_VAL(BLESHELL_MAX_INPUT)
            bleshell_set_conn_handle(event->connect.conn_handle);
#endif
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        /* Connection terminated; resume advertising. */
        bleprph_advertise();
        g_bleprph_connected=0;
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        bleprph_advertise();
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        return 0;

    case BLE_GAP_EVENT_MTU:
        return 0;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);

        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }

    return 0;
}

static void
bleprph_on_reset(int reason)
{
    BLEPRPH_LOG(ERROR, "Rst;%d\n", reason);
}

static void
bleprph_on_sync(void)
{
    int rc;

    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Begin advertising. */
    bleprph_advertise();
}

int
ble_is_connected()
{
#if MYNEWT_VAL(BLEPRPH_ENABLED)
    return g_bleprph_connected;
#else
    return 0;
#endif
}

int
ble_init(uint64_t ble_id)
{
    int rc = 0;

    ble_hs_cfg.reset_cb = bleprph_on_reset;
    ble_hs_cfg.sync_cb = bleprph_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

#if MYNEWT_VAL(BLESHELL_MAX_INPUT)
    rc = bleshell_gatt_svr_init();
    assert(rc == 0);
#endif

    /* Set the default device name. */
    char ble_name[32];
    snprintf(ble_name, sizeof(ble_name), "%s-%04lX", MYNEWT_VAL(BLE_PREFIX),
             (uint32_t)ble_id&0xffffffff);
    BLEPRPH_LOG(INFO, "Setting name to %s\n", ble_name);

    rc = ble_svc_gap_device_name_set(ble_name);
    assert(rc == 0);
    return rc;
}

int
ble_stop()
{
    return ble_gap_adv_stop();
}

void
bleprph_pkg_init(void)
{
    /* Placeholder */
}
