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
#include "hal/hal_gpio.h"
#include "hal/hal_bsp.h"
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif

#if MYNEWT_VAL(BLE_ENABLED)
#include "bleprph/bleprph.h"
#endif

#include <dpl/dpl.h>
#include <uwb/uwb.h>
#include <uwb/uwb_ftypes.h>
#include <nmgr_uwb/nmgr_uwb.h>
#include <nmgr_cmds/nmgr_cmds.h>
//#define DIAGMSG(s,u) printf(s,u)
#ifndef DIAGMSG
#define DIAGMSG(s,u)
#endif

static struct os_callout uwb_callout;

static void
uwb_ev_cb(struct os_event *ev)
{
    struct _nmgr_uwb_instance_t *nmgr = (struct _nmgr_uwb_instance_t *)ev->ev_arg;
    os_callout_reset(&uwb_callout, OS_TICKS_PER_SEC/25);

    if (uwb_nmgr_process_tx_queue(nmgr, 0) == false) {
        return;
    }

    uwb_set_rx_timeout(nmgr->dev_inst, 0);
    uwb_start_rx(nmgr->dev_inst);
}

int main(int argc, char **argv){
    int rc;

    sysinit();
    struct uwb_dev *udev = uwb_dev_idx_lookup(0);

#if MYNEWT_VAL(BLE_ENABLED)
    ble_init(udev->my_short_address);
#endif

    uint32_t utime = os_cputime_ticks_to_usecs(os_cputime_get32());
    printf("{\"utime\": %lu,\"exec\": \"%s\"}\n",utime,__FILE__);
    printf("{\"utime\": %lu,\"msg\": \"device_id = 0x%lX\"}\n",utime,udev->device_id);
    printf("{\"utime\": %lu,\"msg\": \"PANID = 0x%X\"}\n",utime,udev->pan_id);
    printf("{\"utime\": %lu,\"msg\": \"DeviceID = 0x%X\"}\n",utime,udev->uid);
    printf("{\"utime\": %lu,\"msg\": \"partID = 0x%lX\"}\n",
           utime,(uint32_t)(udev->euid&0xffffffff));
    printf("{\"utime\": %lu,\"msg\": \"lotID = 0x%lX\"}\n",
           utime,(uint32_t)(udev->euid>>32));
    printf("{\"utime\": %lu,\"msg\": \"SHR_duration = %d usec\"}\n",utime, uwb_phy_SHR_duration(udev));

    struct _nmgr_uwb_instance_t *nmgr = (struct _nmgr_uwb_instance_t *) uwb_mac_find_cb_inst_ptr(udev, UWBEXT_NMGR_UWB);
    os_callout_init(&uwb_callout, nmgr_cmds_get_eventq(), uwb_ev_cb, nmgr);
    os_callout_reset(&uwb_callout, OS_TICKS_PER_SEC/25);

    uwb_set_rx_timeout(udev, 0);
    uwb_start_rx(udev);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);
    return rc;
}
