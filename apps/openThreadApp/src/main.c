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
#include "sysinit/sysinit.h"
#include "console/console.h"
#include "shell/shell.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "hal/hal_bsp.h"
#ifdef ARCH_sim
#include "mcu/mcu_sim.h"
#endif

#include <uwb/uwb.h>
#include <uwb/uwb_ftypes.h>

#include <openthread/ot_common.h>

//#include <openthread/platform/platform.h>
#include <openthread/platform/uart.h>
#include <openthread/platform/radio.h>
//#include <openthread/types.h>
#include <openthread/instance.h>
#include <openthread/cli.h>
#include <openthread/tasklet.h>
#include <openthread/diag.h>

#include<openthread/platform/openthread-system.h>
static int shell_cmd_ot(int argc, char **argv);

static struct shell_cmd shell_cmd_ot_struct = {
    .sc_cmd = "ot",
    .sc_cmd_func = shell_cmd_ot
};

extern void nrf5AlarmInit(ot_instance_t * ot);

int main(int argc, char **argv)
{
    ot_instance_t *ot;
    otInstance *sInstance;
	struct uwb_dev * inst = uwb_dev_idx_lookup(0);

    PlatformInit(inst);
    shell_cmd_register(&shell_cmd_ot_struct);

    hal_gpio_init_out(LED_BLINK_PIN, 1);

	inst->pan_id = MYNEWT_VAL(PANID);

    printf("{\"device_id\"=\"%lX\"", inst->device_id);
    printf(",\"panid=\"%X\"",inst->pan_id);
    printf(",\"addr\"=\"%X\"",inst->uid);
    printf(",\"euid\"=\"%llX\"",inst->euid);
    printf(",\"part_id\"=\"%lX\"",(uint32_t)(inst->euid&0xffffffff));
    printf(",\"lot_id\"=\"%lX\"}\n",(uint32_t)(inst->euid>>32));
#if OPENTHREAD_ENABLE_MULTIPLE_INSTANCES
    sInstance = otInstanceInit();
#else 
    sInstance = otInstanceInitSingle();
#endif    

    ot = (ot_instance_t*)uwb_mac_find_cb_inst_ptr(inst, UWBEXT_OT);
    assert(ot);
    nrf5AlarmInit(ot);

	ot_post_init(inst, sInstance);

    otCliUartInit(sInstance);

    otDiagInit(sInstance);
	
    while (1) {
        dpl_eventq_run(dpl_eventq_dflt_get());
	}

	assert(0);
	return 0;
}

static int
shell_cmd_ot(int argc, char **argv)
{
    if(argc > 1){
        uint8_t commandSize = 0, count = 0;
        char * command;

        for (int i = 1; i < argc; ++i){
            commandSize += strlen(argv[i]);
            ++commandSize;
        }

        command = (char *)calloc(sizeof(char),commandSize);
        assert(command != NULL);

        for (int i = 1; i < argc; ++i){
            sprintf(&command[count], "%s ", argv[i]);
            count += strlen(argv[i]) + 1;
        }

        command[commandSize-1] = '\n';
        otPlatUartReceived((const uint8_t *)(command),(uint16_t)(strlen(command)));

        memset(command,0,commandSize);
        free(command);
    }
    else
        printf("No Open Thread Command Received\n");

    return 0;

}
