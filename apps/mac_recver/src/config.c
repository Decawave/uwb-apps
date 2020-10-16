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

#include <stdio.h>
#include <os/os.h>
#include <uwb/uwb.h>

#include <mac_recver/config.h>

static void
uwb_config_eventq(struct os_event * ev)
{
    printf("UWB config update\n");
    struct uwb_dev *inst = uwb_dev_idx_lookup(0);
    uwb_mac_config(inst, &inst->config);
}

static int
uwb_config_updated()
{
    static struct os_event ev = {
        .ev_queued = 0,
        .ev_cb = uwb_config_eventq,
        .ev_arg = 0};
    os_eventq_put(os_eventq_dflt_get(), &ev);
    return 0;
}

static struct uwbcfg_cbs uwbcfg_cb = {
    .uc_update = uwb_config_updated
};

int app_conf_init(){
    int ret;

    ret = uwbcfg_register(&uwbcfg_cb);
    if (ret!=0) return ret;

    ret = conf_load();
    return ret;
}
