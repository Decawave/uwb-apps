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

#include <dw1000/dw1000_dev.h>
#include <dw1000/dw1000_hal.h>
#include <dw1000/dw1000_phy.h>
#include <dw1000/dw1000_mac.h>
#include <dw1000/dw1000_ftypes.h>

#include <openthread/ot_common.h>

#include <openthread/platform/platform.h>
#include <openthread/platform/uart.h>
#include <openthread/platform/radio.h>
#include <openthread/types.h>
#include <openthread/instance.h>
#include <openthread/cli.h>
#include <openthread/tasklet.h>
#include <openthread/diag.h>

static int shell_cmd_ot(int argc, char **argv);

static struct shell_cmd shell_cmd_ot_struct = {
    .sc_cmd = "ot",
    .sc_cmd_func = shell_cmd_ot
};


int main(int argc, char **argv){
    otInstance *sInstance;
	dw1000_dev_instance_t * inst = hal_dw1000_inst(0);

    PlatformInit(inst);

    shell_cmd_register(&shell_cmd_ot_struct);

    hal_gpio_init_out(LED_BLINK_PIN, 1);

	inst->PANID = MYNEWT_VAL(PANID);
	inst->my_short_address = 0xDECA;

    printf("device_id = 0x%lX\n",inst->device_id);
    printf("PANID = 0x%X\n",inst->PANID);
    printf("DeviceID = 0x%X\n",inst->my_short_address);
    printf("partID = 0x%lX\n",inst->partID);
    printf("lotID = 0x%lX\n",inst->lotID);
    printf("xtal_trim = 0x%X\n",inst->xtal_trim);
    printf("Long add = 0x%llX\n",inst->my_long_address);

	sInstance = otInstanceInit();
	ot_post_init(inst, sInstance);

    otCliUartInit(sInstance);

    otDiagInit(sInstance);
	
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
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


