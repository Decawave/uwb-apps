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

6. On the first DWM1001-DEV board build the Two-Way-Ranging (twr_tag) application for the DWM1001 module. The run command compiles the project and loads the image on the target platform.

(executed from the mynewt-dw1000-app directory).

```no-highlight

newt target create twr_tag
newt target set twr_tag app=apps/twr_tag
newt target set twr_tag bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set twr_tag build_profile=debug 
newt run twr_tag 0

```

7. On a second DWM1001-DEV board build the node side of the Two-Way-Ranging (twr_node) application as follows. 

(executed from the mynewt-dw1000-app directory).

```no-highlight

newt target create twr_node 
newt target set twr_node app=apps/twr_node
newt target set twr_node bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set twr_node build_profile=debug 
newt run twr_node 0

```


8. Both examples are configured to use the Segger RTT console interface. This is covered within the mynewt tutorials/Tooling/SeggerRTT (http://mynewt.apache.org/latest/tutorials/tooling/segger_rtt.html). To launch the console simply telnet localhost 19021. Note at time of writing the newt tools does not support multiple connect dwm1001-dev devices. So it is recomended that you connect twr_tag and twr_node examples to different computers or at least the twr_tag to an external battery. If all goes well you should see the twr_node example stream range information on the console. 

(executed from the mynewt-dw1000-app directory).

```no-highlight

nc localhost 19021

```

9. This repo also contains the project twr_node_json. This project works in conjunction with the matlab utility folder and exposes some internal variables to the matlab workspace over TCP. The internal variables are encapsulated within JSON string and streamed over TCP. These examples are intended as a how-to-guide. This JSON API approach is an extensible API and can be augmented as needed. 

```no-highlight

newt target create twr_node_json 
newt target set twr_node_json app=apps/twr_node_json
newt target set twr_node_json bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set twr_node_json build_profile=debug 
newt run twr_node_json 0

```

On your console you should see output similar to shown below;

```no-highlight

{"rxdiag": {"fp_idx": 48075,"fp_amp": 7438,"rx_std": 68,"preamble_cnt": 122}}
{"utime": 2104,"fp_idx": 48075,"Tp": 1138}
{"ss_twr": [{"fctrl": 34881,"seq_num": 221,"PANID": 57034,"dst_address": 4660,"src_address": 17185,"code": 6,"reception_timestamp": 2963524364,"transmission_timestamp": 3030698560,"request_timestamp": 4088479744,"response_timestamp": 4155654358},{"fctrl": 34881,"seq_num": 221,"PANID": 57034,"dst_address": 4660,"src_address": 17185,"code": 8,"reception_timestamp": 4155654358,"transmission_timestamp": 4222828608,"request_timestamp": 3030632960,"response_timestamp": 3097807908}]}
{"cir": {"fp_idx": 48047,"real": [76,94,142,-46,-84,-66,-17,-58,1,-15,136,-66,249,138,-149,-121,-34,-33,30,62,41,89,-64,27,-77,-76,-23,64,74,157,-50,111,62,69,61,168,71,70,8,-26,-40,102,98,-171,-19,56,29,29,52,-53,-137,60,160,10,-43,40,130,-60,-124,-72,-127,-86,10,-150,4,-59,-95,82,-8,-84,-51,100,133,57,-50,208,328,159,106,103,31,52,162,8,-43,68,-91,91,-36,46,144,-20,30,123,86,118,39,-4,-87,-74,-74,23,69,73,137,-24,-38,87,-5,-104,-13,-192,-48,-12,9,-41,-129,165,-20,-120,-36,148,154,7,-161,-137,92,130,122,-59,-124,187,64,-18,-84,-229,-32,44,-149,132,123,-140,85,87,10,-13,143,196,12,25,123,-585,-3057,-5008,-6048,-900,4095,2909,-118,-998,-676,-372,-135,439,871,63,-757,-911,-1247,-2210,-2877,-1574,-1400,-147,703,1323,694,426,219,-94,-494,-10,692,1577,592,-282,-810,-182,984,929,398,-15,5,433,428,339,199,349,540,953,327,-157,-555,-219,352,514,40,120,-35,158,300,400,84,-349,27,60,-137,-113,-7,319,423,254,296,313,66,1,115,431,359,98,-111,56,123,305,281,-31,-304,26,65,-74,-106,78,189,14,-94,5,166,156,75,39,159,27,-144,-116,21,9],"imag": [-148,34,44,-151,-106,-39,79,4,142,243,-51,-205,-94,-63,-80,-143,95,23,-21,1,-39,-268,-12,-107,5,29,79,20,-57,-75,27,34,-65,69,35,111,107,49,-106,33,10,-136,-16,102,11,-79,-29,119,-126,-6,154,67,-2,-27,-95,225,-103,-73,38,68,85,109,12,-121,-47,41,106,26,49,73,108,90,94,259,53,88,-19,14,-102,-22,-59,-373,-298,12,-19,81,-41,54,92,-101,-47,81,95,-68,-53,31,-14,34,176,72,111,89,-1,169,166,211,145,93,36,33,-76,-65,8,-287,-69,32,-97,-27,85,-12,-145,-200,-76,-18,196,106,-46,72,6,278,159,-14,-143,-6,33,-20,198,318,151,65,15,-102,-19,-51,53,181,-20,-77,-106,-35,-455,-5155,-7118,-6820,-5345,-676,-147,-2536,-960,1293,694,-207,-82,1011,1556,39,-1467,-569,1450,3653,3217,617,-694,81,1259,946,237,503,250,-8,12,272,522,-149,-216,109,479,180,-3,-232,438,1030,653,380,-153,445,826,373,-22,-583,-299,145,580,429,-224,-512,-146,468,469,435,118,172,266,363,376,471,198,350,339,305,167,-137,157,295,163,270,14,140,119,-52,-86,-170,-240,-319,-96,203,151,149,97,-26,76,-73,35,-35,5,140,364,186,178,26,64,-85,55,193,324,262]}}
{"rxdiag": {"fp_idx": 48047,"fp_amp": 8078,"rx_std": 68,"preamble_cnt": 122}}
{"utime": 2113,"fp_idx": 48047,"Tp": 1116}

```


This is human and machine readable JSON format. You can now use stats.m as an example to study the statistical performance of the platform or read_cir.m to study the channel impulse response. Again this is an extensible API that you augment as needed. The Mynewt OS provides native support for JSON encoding and parsing, as such this API can also be made bidirectional. 


