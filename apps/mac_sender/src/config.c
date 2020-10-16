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

#include <mac_sender/config.h>

static char *app_conf_get(int argc, char **argv, char *val, int val_len_max);
static int app_conf_set(int argc, char **argv, char *val);
static int app_conf_commit(void);
static int app_conf_export(void (*export_func)(char *name, char *val), enum conf_export_tgt tgt);
static int uwb_config_updated(void);

static struct {
    int16_t send_period_x100;
} g_app_conf_num;

static struct{
    char send_period_x100[6];
} g_app_conf_str= {
    .send_period_x100 = "100"
};

static struct conf_handler g_app_conf_handler = {
    .ch_name    = "app",
    .ch_get     = app_conf_get,
    .ch_set     = app_conf_set,
    .ch_commit  = app_conf_commit,
    .ch_export  = app_conf_export,
};

static struct uwbcfg_cbs g_uwbcfg_cb = {
    .uc_update = uwb_config_updated
};

static char *
app_conf_get(int argc, char **argv, char *val, int val_len_max)
{
    printf("app_conf_get\n");
    if (argc == 1) {
        if (!strcmp(argv[0], "send_period_x100"))  return g_app_conf_str.send_period_x100;
    }
    return NULL;
}

static int
app_conf_set(int argc, char **argv, char *val)
{
    printf("app_conf_set\n");
    if (argc == 1) {
        if (!strcmp(argv[0], "send_period_x100")) {
            return CONF_VALUE_SET(val, CONF_STRING, g_app_conf_str.send_period_x100);
        }
    }
    return -1;
}

static int
app_conf_commit(void)
{
    printf("app_conf_commit: ");
    int ret = conf_value_from_str(g_app_conf_str.send_period_x100, CONF_INT16,
                        (void*)&(g_app_conf_num.send_period_x100), 0);
    if(ret != 0) printf("failed\n");
    else printf("done\n");
    return ret;
}

static int
app_conf_export(void (*export_func)(char *name, char *val),
             enum conf_export_tgt tgt)
{
    printf("app_conf_export\n");
    export_func("app/send_period_x100", g_app_conf_str.send_period_x100);
    return 0;
}

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

int app_conf_init(){
    int ret;
    ret = conf_register(&g_app_conf_handler);
    if (ret!=0) return ret;

    
    ret = uwbcfg_register(&g_uwbcfg_cb);
    if (ret!=0) return ret;

    ret = conf_load();
    return ret;
}

int16_t app_conf_get_send_period_x100(){
    return g_app_conf_num.send_period_x100;
}