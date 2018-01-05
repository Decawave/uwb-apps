<!--
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#  KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
-->

# Decawave DW1000 Applications 

## Overview

This distribution contains the example applications for the dw1000 IR-UWB transceiver within the mynewt-OS. The dw1000 device driver model is integrated into the mynewt-OS (https://github.com/decawave/mynewt-dw1000-core). This driver includes native support for a 6lowPAN stack, Ranging Services, and Location Services, etc. Mynewt and it's tools build environment newt and management tools newtmgt create a powerful environment and deploying large-scale distributions within IoT.

For these examples, we leverage the Decawave dwm1001 module and dwm1001-dev kit. The dwm1001 includes a nrf52832 and the dw1000 transceiver. The dwm1001-dev is a breakout board that supports a Seggar OB-JLink interface with RTT support. The mynewt build environment provides a clean interface for maintaining platform agnostics distributions. The dwm1001-dev and the examples contained herein provide a clean out-of-the-box experience for UWB Location Based Services.

Warning: The dwm1001 comes flashed with a UWB Location Based Services stack. This distribution repurposes the hardware and is not intended to replace the functionality of the shipped stack. This distribution is intended to be a starting point for evaluating and developing one's own such stacks. 


## Project Status
This project is destined to be up-streamed into the mynewt repo Q1 2018:

* Single-Sides Two-Way-Ranging and Symetric Double-Sided Two-Way-Ranging
*   twr_node:
*   twr_tag  
* blinky

## Building

1. Download and install Apache Newt.

You will need to download the Apache Newt tool, as documented in the [Getting Started Guide](http://mynewt.apache.org/os/get_started/introduction/). 

Prerequisites: You should follow the generic tutorials at https://mynewt.apache.org/latest/os/tutorials/tutorials/. This example is an extension of the basic Blinky example.

2. Download the DW1000 Mynewt apps.

```no-highlight
    git clone https://github.com/decawave/mynewt-dw1000-apps.git
    cd mynewt-dw1000-apps
```


3. Download the Apache Mynewt Core and Mynewt DW1000 Core package.

Currently mynewt-dw1000-core is a private repo and as such requires an extra step to provide authorization. See (https://mynewt.apache.org/latest/os/tutorials/repo/private_repo/) for a how to guide. Note with newt version 1.2, I have only had sucess with the project.yml approach to private repos. It is also necessary delete the ./repos director after an error has occured. Don't forget to save the .patch folder.

```no-highlight
    $ newt install
```
Apply the C99 extendsion patch to the apache-mynewt-core distribution. This is contained in the file ./repos/.patches/apache-mynewt-core.patch

```no-highlight
    cd ./repos/
    git apply /.patches/apache-mynewt-core.patch
```

If all goies well the file ./repos/apache-mynewt-core/compiler/arm-none-eabi-m4/compiler.yml should now contain -fms-extensions -std=c11

```no-highlight
    compiler.flags.base: -mcpu=cortex-m4 -mthumb-interwork -mthumb -Wall -Werror -fno-exceptions -ffunction-sections -fdata-sections -fms-extensions -std=c11
```

4. To ereae the default flash image that shipped with the DWM1001.

```no-highlight
$ JLinkExe -device nRF52 -speed 4000 -if SWD
SEGGER J-Link Commander V5.12c (Compiled Apr 21 2016 16:05:51)
DLL version V5.12c, compiled Apr 21 2016 16:05:45

Connecting to J-Link via USB...O.K.
Firmware: J-Link OB-SAM3U128-V2-NordicSemi compiled Mar 15 2016 18:03:17
Hardware version: V1.00
S/N: 682863966
VTref = 3.300V


Type "connect" to establish a target connection, '?' for help
J-Link>erase
Cortex-M4 identified.
Erasing device (0;?i?)...
Comparing flash   [100%] Done.
Erasing flash     [100%] Done.
Verifying flash   [100%] Done.
J-Link: Flash download: Total time needed: 0.363s (Prepare: 0.093s, Compare: 0.000s, Erase: 0.262s, Program: 0.000s, Verify: 0.000s, Restore: 0.008s)
Erasing done.
J-Link>exit
$ 
```

4. Build the new bootloader applicaiton for the DWM1001 taregt.

(executed from the mynewt-dw1000-app directory).

```no-highlight

    newt target create dwm1001_boot
    newt target set dwm1001_boot app=@apache-mynewt-core/apps/boot
    newt target set dwm1001_boot bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
    newt target set dwm1001_boot build_profile=optimized
    newt build dwm1001_boot
    newt create-image dwm1001_boot 1.0.0
    newt load dwm1001_boot

```

5. On the first dwm1001-dev Build the Two Way Ranging (twr_tag) Applicaitons for the DWM1000 hardware platform. The mynewt-dw1000-app is already configured for the dwm1001 target, however for completeness the first two command below assign and create the target platform. The load command compiled the project and loads the image on the target platform.

(executed from the mynewt-dw1000-app directory).

```no-highlight

    newt target set twr_tag bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
    newt target set twr_tag build_profile=debug
    newt load twr_tag

```

6. On a second dwm1001-dev build the master size of the Two Way Ranging (twr_node) Applicaitons as follows. The run command compiled the project and loads the image on the target platform.

(executed from the mynewt-dw1000-app directory).

```no-highlight

    newt target set twr_node bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
    newt target set twr_node build_profile=debug
    newt run twr_node

```
To switch from Single-Side to Symetric-Double-Size, simply comment ./twr_node/main.c as follows: 
```no-highlight

   //dw1000_rng_request(inst, 0x4321, DWT_SDS_TWR);
   dw1000_rng_request(inst, 0x4321, DWT_SS_TWR);

```

7. Both examples are configured to use the Segger RTT console interface. This is covered within the mynewt tutorials/Tooling/SeggarRTT (https://mynewt.apache.org/latest/os/tutorials/segger_rtt/). To launch the console simply  telnet localhost 19021. Note time of writing the newt tools does not support multiple connect dwm1001-dev devices. So it is recomended that you connect ss_twr_slave and sw_twr_master examples to different computers or at least the ss_twr_slave to an external battery. If all going well you should see the sw_twr_master example stream range information on the console. 

(executed from the mynewt-dw1000-app directory).

```no-highlight

    telnet localhost 19021

```
