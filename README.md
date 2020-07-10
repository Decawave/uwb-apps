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

# Decawave UWB Applications
[![Build Status](https://travis-ci.org/Decawave/uwb-apps.svg?branch=master)](https://travis-ci.org/Decawave/uwb-apps)

## Overview

This distribution contains the example applications for the DW1000 UWB transceiver.

## Getting hardware

* DWM1001   from https://www.decawave.com/products/dwm1001-module
* lps2mini  from https://lohmega.com

## Getting started

The remainder of this README.md shows how to bring up the elementary twr_node/twr_tag examples.

1. Download and install Apache Newt.

You will need to download the Apache Newt tool, as documented in the [Getting Started Guide](http://mynewt.apache.org/latest/get_started/index.html).

Prerequisites: You should follow the generic tutorials at http://mynewt.apache.org/latest/tutorials/tutorials.html, particularly the basic Blinky example that will guide you through the basic setup.

2. Download the UWB apps repository.

```no-highlight
    git clone git@github.com:Decawave/uwb-apps.git
    cd uwb-apps
```

3. Running the ```newt upgrade``` command downloads the apache-mynewt-core, decawave-uwb-core, decawave-uwb-dwXXXX driver(s) repo, and mynewt-timescale-lib packages, these are dependent repos of the decawave-uwb-apps project and are automatically checked-out by the newt tools.

```no-highlight
$ newt upgrade
# Depending on what version of the newt tool you use you
# may have to manually remove git-but artifacts and then rerun
# upgrade. I.e. only if you see an error like:
# "Error: Error updating "mcuboot"..." do:
rm repos/mcuboot/ext/mbedtls/include/mbedtls/ -rf
newt upgrade
```

To see if you have access to other driver repos, run the setup.sh
script under repos/decawave-uwb-core like so:

```
repos/decawave-uwb-core/setup.sh
# Rerun newt install
newt install
```

Apply any patches to apache-mynewt-core:

```
cd repos/apache-mynewt-core/
git apply ../decawave-uwb-core/patches/apache-mynewt-core/mynewt_1_7_0_*
cd -
```

4. To erase the default flash image that shipped with the DWM1001.

```no-highlight
$ JLinkExe -device nRF52 -speed 4000 -if SWD
J-Link>erase
J-Link>exit
$
```

or if you have nrfjprog ([Nordic Cmd Tools](https://www.nordicsemi.com/Software-and-tools/Development-Tools/nRF-Command-Line-Tools/Download)) installed:

```
    $ nrfjprog -f NRF52 -e
```


5. Build the new bootloader applicaiton for the DWM1001 target.

(executed from the mynewt-dw1000-app directory).

```no-highlight

newt target create dwm1001_boot
newt target set dwm1001_boot app=@mcuboot/boot/mynewt
newt target set dwm1001_boot bsp=@decawave-uwb-core/hw/bsp/dwm1001
newt target set dwm1001_boot build_profile=optimized
newt build dwm1001_boot
newt load dwm1001_boot

```

6. On the first DWM1001-DEV board build the Two-Way-Ranging (twr_tag_tdma) application for the DWM1001 module. The run command compiles the project and loads the image on the target platform.

(executed from the decawave-uwb-apps directory).

```no-highlight

newt target create twr_tag_tdma
newt target set twr_tag_tdma app=apps/twr_tag_tdma
newt target set twr_tag_tdma bsp=@decawave-uwb-core/hw/bsp/dwm1001
newt target set twr_tag_tdma build_profile=debug
newt run twr_tag_tdma 0

```

7. On a second DWM1001-DEV board build the node side of the Two-Way-Ranging (twr_node_tdma) application as follows.

(executed from the decawave-uwb-apps directory).

```no-highlight

newt target create twr_node_tdma
newt target set twr_node_tdma app=apps/twr_node_tdma
newt target set twr_node_tdma bsp=@decawave-uwb-core/hw/bsp/dwm1001
newt target set twr_node_tdma build_profile=debug
newt target amend twr_node_tdma syscfg=LOG_LEVEL=1:UWBCFG_DEF_ROLE='"0x1"'
newt run twr_node_tdma 0

```


8. Both examples are configured to use the Segger RTT console interface. This is covered within the mynewt tutorials/Tooling/SeggerRTT (http://mynewt.apache.org/latest/tutorials/tooling/segger_rtt.html). To launch the console simply telnet localhost 19021. Note at time of writing the newt tools does not support multiple connect dwm1001-dev devices. So it is recomended that you connect twr_tag_tdma and twr_node_tdma examples to different computers or at least the twr_tag_tdma to an external battery. If all goes well you should see the twr_node_tdma example stream range information on the console. 

(executed from the decawave-uwb-apps directory).

```no-highlight

nc localhost 19021

```
