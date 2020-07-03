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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "sysinit/sysinit.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "bleshell/bleshell.h"
#include "os/endian.h"
#include "shell/shell.h"

void shell_process_command(char *line, struct streamer *streamer);

/* ble shell attr read handle */
uint16_t g_bleshell_attr_read_handle;

/* ble shell attr write handle */
uint16_t g_bleshell_attr_write_handle;

/* Pointer to a console buffer */
static char *console_buf = 0;

static int streamer_ble_write(struct streamer *streamer, const void *src, size_t len);
static int streamer_ble_vprintf(struct streamer *streamer, const char *fmt, va_list ap);

static const struct streamer_cfg streamer_cfg = {
    .write_cb = streamer_ble_write,
    .vprintf_cb = streamer_ble_vprintf,
};

static struct streamer ble_streamer = {
    .cfg = &streamer_cfg,
};


uint16_t g_console_conn_handle = 0;

/**
 * The vendor specific "bleshell" service consists of one write no-rsp characteristic
 * and one notification only read charateristic
 *     o "write no-rsp": a single-byte characteristic that can be written only
 *       over a non-encrypted connection
 *     o "read": a single-byte characteristic that can always be read only via
 *       notifications
 */

/* {6E400001-B5A3-F393-E0A9-E50E24DCCA9E} */
const ble_uuid128_t gatt_svr_svc_shell_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);

/* {6E400002-B5A3-F393-E0A9-E50E24DCCA9E} */
const ble_uuid128_t gatt_svr_chr_shell_write_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);


/* {6E400003-B5A3-F393-E0A9-E50E24DCCA9E} */
const ble_uuid128_t gatt_svr_chr_shell_read_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

static int
gatt_svr_chr_access_shell_write(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /* Service: shell */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_shell_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) { {
            .uuid = &gatt_svr_chr_shell_read_uuid.u,
            .val_handle = &g_bleshell_attr_read_handle,
            .access_cb = gatt_svr_chr_access_shell_write,
            .flags = BLE_GATT_CHR_F_NOTIFY,
        }, {
            /* Characteristic: Write */
            .uuid = &gatt_svr_chr_shell_write_uuid.u,
            .access_cb = gatt_svr_chr_access_shell_write,
            .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            .val_handle = &g_bleshell_attr_write_handle,
        }, {
            0, /* No more characteristics in this service */
        } },
    },

    {
        0, /* No more services */
    },
};

static int
gatt_svr_chr_access_shell_write(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int i, j;
    struct os_mbuf *om = ctxt->om;
    char buf[MYNEWT_VAL(BLESHELL_MAX_INPUT)] = {0};
    char *bufp = buf;
    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            j = 0;
            while(om) {
                for (i=0;i<om->om_len && j<sizeof(buf);i++,j++) {
                    *(bufp++) = (((char *)om->om_data)[i]);
                }
                om = SLIST_NEXT(om, om_next);
            }
            j--;
            while (buf[j] == '\n') buf[j--] = 0;
            printf("from ble: '%s'\n", buf);
            shell_process_command(buf, &ble_streamer);
            return 0;
        default:
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
    }
}

/**
 * bleshell GATT server initialization
 *
 * @param eventq
 * @return 0 on success; non-zero on failure
 */
int
bleshell_gatt_svr_init(void)
{
    int rc;

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        goto err;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

err:
    return rc;
}

/**
 * Reads console and sends data over BLE
 */

void
bleshell_shell_write(const char *str, int cnt)
{
    int off;
    struct os_mbuf *om;

    if (!str) return;
    if (!cnt) return;
    if (!console_buf) return;
    if (!g_console_conn_handle) return;

    printf("to ble: '%s'\n", str);

    off = 0;
    while (1) {
        int to_read = cnt - off;
        to_read = (to_read > MYNEWT_VAL(BLESHELL_MAX_INPUT)) ? MYNEWT_VAL(BLESHELL_MAX_INPUT) : to_read;
        memcpy(console_buf, str + off, to_read);
        off += to_read;

        om = ble_hs_mbuf_from_flat(console_buf, off);
        if (!om) {
            return;
        }
        ble_gattc_notify_custom(g_console_conn_handle,
                                g_bleshell_attr_read_handle, om);
        off = 0;
        break;
    }
}

static int
streamer_ble_write(struct streamer *streamer, const void *src, size_t len)
{
    bleshell_shell_write(src, len);
    return 0;
}

static int
streamer_ble_vprintf(struct streamer *streamer,
                 const char *fmt, va_list ap)
{
    int rc;
    struct os_mbuf *om;
    int num_bytes = vsnprintf(console_buf, MYNEWT_VAL(BLESHELL_MAX_INPUT),
                              fmt, ap);

    printf("to ble: '%s'\n", console_buf);
    om = ble_hs_mbuf_from_flat(console_buf, num_bytes);
    if (!om) {
        return 0;
    }
    rc = ble_gattc_notify_custom(g_console_conn_handle,
                            g_bleshell_attr_read_handle, om);
    printf("notify rc: %d\n", rc);
    assert(rc == 0);

    return num_bytes;
}

/**
 * Sets the global connection handle
 *
 * @param connection handle
 */
void
bleshell_set_conn_handle(uint16_t conn_handle) {
    g_console_conn_handle = conn_handle;
}

/**
 * BLEshell console initialization
 *
 * @param Maximum input
 */
void
bleshell_init(void)
{
    int rc = 0;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    SYSINIT_PANIC_ASSERT(rc == 0);

    console_buf = malloc(MYNEWT_VAL(BLESHELL_MAX_INPUT));
    SYSINIT_PANIC_ASSERT(console_buf != NULL);
}
