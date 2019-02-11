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

This distribution contains the example applications for the DW1000 IR-UWB transceiver within the mynewt-OS. The dw1000 device driver model is integrated into the mynewt-OS (https://github.com/decawave/mynewt-dw1000-core). This driver includes native support for a 6lowPAN stack, Ranging Services, and Location Services, etc. Mynewt and its build environment tool newt and management tool newtmgt creates a powerful environment for deploying large-scale distributions within IoT.

For these examples, we leverage the Decawave dwm1001 module and dwm1001-dev kit. The DWM1001 includes a nrf52832 and the DW1000 transceiver. The DWM1001-DEV is a breakout board that supports a Seggar OB-JLink interface with RTT support. The mynewt build environment provides a clean interface for maintaining platform agnostic distributions. The DWM1001-DEV and the examples contained herein provide a clean out-of-the-box experience for UWB Location Based Services.

Warning: The DWM1001 comes flashed with a UWB Location Based Services stack. This distribution repurposes the hardware and is not intended to replace the functionality of the shipped stack. This distribution is intended to be a starting point for evaluating and developing one's own stacks. 

## Getting support

Project discussion board, http://decawave.slack.com

## Getting hardware

* DWM1001   from https://www.decawave.com/products/dwm1001-module
* DWM1002   from https://decawave.com (coming soon)
* DWM1003   from https://decawave.com (coming soon)
* locomo    from http://www.ubitraq.com
* lps2mini  from https://loligoelectronics.com
* lps2nano  from https://loligoelectronics.com

## Getting started

The remainder of this README.md shows how to bring up the elementary twr_node/twr_tag examples for the DWM1001_DEV kit.

1. Download and install Apache Newt.

You will need to download the Apache Newt tool, as documented in the [Getting Started Guide](http://mynewt.apache.org/latest/get_started/index.html). 

Prerequisites: You should follow the generic tutorials at http://mynewt.apache.org/latest/tutorials/tutorials.html, particularly the basic Blinky example that will guide you through the basic setup.

2. Download the DW1000 Mynewt apps.

```no-highlight
    git clone git@github.com:Decawave/mynewt-dw1000-apps.git
    cd mynewt-dw1000-apps
```

3. Running the newt install command downloads the apache-mynewt-core, mynewt-dw1000-core, and mynewt-timescale-lib packages, these are dependent repos of the mynewt-dw1000-apps project and are automatically checked-out by the newt tools.

```no-highlight
    $ newt install
```

4. To erase the default flash image that shipped with the DWM1001.

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

5. Build the new bootloader applicaiton for the DWM1001 target.

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

6. On the first DWM1001-DEV board build the Two-Way-Ranging (twr_tag_tdma) application for the DWM1001 module. The run command compiles the project and loads the image on the target platform.

(executed from the mynewt-dw1000-app directory).

```no-highlight

newt target create twr_tag_tdma
newt target set twr_tag_tdma app=apps/twr_tag_tdma
newt target set twr_tag_tdma bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set twr_tag_tdma build_profile=debug 
newt run twr_tag_tdma 0

```

7. On a second DWM1001-DEV board build the node side of the Two-Way-Ranging (twr_node_tdma) application as follows. 

(executed from the mynewt-dw1000-app directory).

```no-highlight

newt target create twr_node_tdma 
newt target set twr_node_tdma app=apps/twr_node_tdma
newt target set twr_node_tdma bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set twr_node_tdma build_profile=debug 
newt run twr_node_tdma 0

```


8. Both examples are configured to use the Segger RTT console interface. This is covered within the mynewt tutorials/Tooling/SeggerRTT (http://mynewt.apache.org/latest/tutorials/tooling/segger_rtt.html). To launch the console simply telnet localhost 19021. Note at time of writing the newt tools does not support multiple connect dwm1001-dev devices. So it is recomended that you connect twr_tag_tdma and twr_node_tdma examples to different computers or at least the twr_tag_tdma to an external battery. If all goes well you should see the twr_node_tdma example stream range information on the console. 

(executed from the mynewt-dw1000-app directory).

```no-highlight

telnet localhost 19021

```
