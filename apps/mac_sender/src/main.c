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

#include <os/os.h>
#include <sysinit/sysinit.h>
#include <uart/uart.h>

#include <uwb/uwb.h>
#include <uwb/uwb_ftypes.h>
#include <uwbcfg/uwbcfg.h>

#include <mac_sender/config.h>

static struct uart_dev *g_uart_dev;
static struct dpl_callout g_tx_callout;

static int
uart_console_tx_char(void *arg)
{
    return 0;
}

static int
uart_console_rx_char(void *arg, uint8_t byte)
{
    return 0;
}

static void 
uart_print(struct uart_dev *dev, char *str){
    while(*str != 0){
        uart_blocking_tx(dev, *str);
        str++;
    }
}

static void tx_callout_cb(struct dpl_event *ev)
{
    static ieee_blink_frame_t blink_frame = {
        .fctrl          = 0xC5,                 /* frame type (0xC5 for a blink) using 64-bit addressing */
        .seq_num        = 0,                    /* sequence number, incremented for each new frame. */
        .long_address   = 0x1234567890DECA90,   /* device ID */
    };

    assert(ev != NULL);
    struct uwb_dev *udev = (struct uwb_dev *)dpl_event_get_arg(ev);
    if(udev == NULL) printf("TX callout: No device found");

    char str[30];
    sprintf(str, "seqno: %d\r\n", blink_frame.seq_num);
    uart_print(g_uart_dev, str);

    uwb_write_tx(udev, blink_frame.array, 0, sizeof(ieee_blink_frame_t));
    uwb_write_tx_fctrl(udev, sizeof(ieee_blink_frame_t), 0);
    uwb_start_tx(udev);

    blink_frame.seq_num++;
    
    dpl_callout_reset(&g_tx_callout,  OS_TICKS_PER_SEC*app_conf_get_send_period_x100()/100.f);
}

bool
tx_begins_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    uart_print(g_uart_dev, "TX begin callback\r\n");
    return true;
}

bool
tx_complete_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    uart_print(g_uart_dev, "TX complete callback\r\n");
    return true;
}

bool
rx_complete_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    uart_print(g_uart_dev, "RX complete callback\r\n");
    return true;
}

bool
rx_timeout_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    uart_print(g_uart_dev, "RX timeout callback\r\n");
    return true;
}

bool
error_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    uart_print(g_uart_dev, "TX/RX error callback\r\n");
    return true;
}

bool
reset_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    uart_print(g_uart_dev, "Reset interface callback\r\n");
    return true;
}

bool
complete_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    uart_print(g_uart_dev, "Completion event interface callback\r\n");
    return true;
}

bool
sleep_cb(struct uwb_dev * inst, struct uwb_mac_interface * cbs)
{
    uart_print(g_uart_dev, "Wakeup event interface callback\r\n");
    return true;
}

static struct uwb_mac_interface uwb_mac_if = {
    .id = UWBEXT_APP0,

    .tx_begins_cb = tx_begins_cb,
    .tx_complete_cb = tx_complete_cb,
    
    .rx_complete_cb = rx_complete_cb,
    .rx_timeout_cb = rx_timeout_cb,
    
    .tx_error_cb = error_cb,
    .rx_error_cb = error_cb,

    .reset_cb = reset_cb,
    .complete_cb = complete_cb,
    .sleep_cb = sleep_cb,
};

int main(int argc, char **argv){
    int rc;
    struct uwb_dev *udev;
    uint32_t utime;

    sysinit();
    app_conf_init();

    struct uart_conf uc = {
        .uc_speed = MYNEWT_VAL(COM_UART_BAUD),
        .uc_databits = 8,
        .uc_stopbits = 1,
        .uc_parity = UART_PARITY_NONE,
        .uc_flow_ctl = UART_FLOW_CTL_NONE,
        .uc_tx_char = uart_console_tx_char,
        .uc_rx_char = uart_console_rx_char,
    };
    g_uart_dev = (struct uart_dev *)os_dev_open(MYNEWT_VAL(COM_UART_DEV),
        OS_TIMEOUT_NEVER, &uc);
    if (!g_uart_dev) {
        printf("UART device: not found");
        return -1;
    }

    udev  = uwb_dev_idx_lookup(0);
    if (!udev) {
        printf("UWB device: not found");
        return -1;
    }
    utime = dpl_cputime_ticks_to_usecs(dpl_cputime_get32());
    printf("{\"utime\": %lu,\"exe\": \"%s\"}\n",                    utime,  __FILE__);
    printf("{\"utime\": %lu,\"msg\": \"device_id    = 0x%lX\"}\n",  utime,  udev->device_id);
    printf("{\"utime\": %lu,\"msg\": \"PANID        = 0x%X\"}\n",   utime,  udev->pan_id);
    printf("{\"utime\": %lu,\"msg\": \"DeviceID     = 0x%X\"}\n",   utime,  udev->uid);
    printf("{\"utime\": %lu,\"msg\": \"partID       = 0x%lX\"}\n",  utime,  (uint32_t)(udev->euid&0xffffffff));
    printf("{\"utime\": %lu,\"msg\": \"lotID        = 0x%lX\"}\n",  utime,  (uint32_t)(udev->euid>>32));
    printf("{\"utime\": %lu,\"msg\": \"SHR_duration = %d usec\"}\n",utime,  uwb_phy_SHR_duration(udev));
    uwb_mac_append_interface(udev, &uwb_mac_if);

    dpl_callout_init(&g_tx_callout, dpl_eventq_dflt_get(), tx_callout_cb, udev);
    int16_t send_period = app_conf_get_send_period_x100();
    dpl_callout_reset(&g_tx_callout, OS_TICKS_PER_SEC*send_period/100.0f);
    printf("{\"utime\": %lu,\"msg\": \"send_period  = %d/100sec\"}\n",utime, send_period);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}
