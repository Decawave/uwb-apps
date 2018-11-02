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

# Decawave TWR_TDMA Example

## Overview

The twr_node_tdma and twr_tag_tdma illustrate the capabilities of the underlying mynewt-dw1000-core library. The default behavior divides the TDMA_PERIOD into TDMA_NSLOTS and allocates slots to a single ranging task. Clock calibration and timescale compensation are enabled within the twr_tag_tdma application; this has the advantage of permitting single-sided two range exchange thus reducing the channel utilization. 

Another advantege of wireless synchronization is the ability to turn-on transeiver is a just-in-time fashion thereby reducing the power comsumption. In these examples transeiver is turned on for approximatly 190 usec per each slot. 

### Under-the-hood

The mynewt-dw1000-core driver implements the MAC layers and exports a MAC extension interface for additional services. One such service is ranging (./lib/rng). The ranging services through the extension interface expose callback to various events within the ranging process. One such callback is the complete_cb which marks the successful completion of the range request. In these examples, we attach to the complete_cb and perform subsequent processing. The available callbacks are defined in the struct _dw1000_mac_interface_t which is defined in dw1000_dev.h

This example also illustrates the clock calibration packet (CCP) and time division multiple access (TDMA) services. Both of which also bind to the MAC interface. In a synchronous network, the CCP service establishes the metronome through superframes transmissions. All epochs are derived from these superframes. The TDMA service divides the superframe period into slots and schedules event about each slot. The transceiver controls the precise timing of all frames to the microsecond. The operating system loosely schedules an event in advance of the desired epoch and this event issues a delay_start request to the transceiver. This loose/tight timing relationship reduces the timing requirements on the OS and permits the dw1000 to operate at optimum efficiency.

1. To erase the default flash image that shipped with the DWM1001 boards.

```no-highlight
$ JLinkExe -device nRF52 -speed 4000 -if SWD
J-Link>erase
J-Link>exit
$ 
```

2. On 1st dwm1001-dev board build the TDMA node (twr_node_tdma) applications for the DWM1001 module. 

```no-highlight

newt target create twr_node_tdma
newt target set twr_node_tdma app=apps/twr_node_tdma
newt target set twr_node_tdma bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set twr_node_tdma build_profile=debug
newt run twr_node_tdma 0

```

3. On 2nd dwm1001-dev board build the TDMA tag (twr_tag_tdma) applications for the DWM1001 module. 

```no-highlight

newt target create twr_tag_tdma
newt target set twr_tag_tdma app=apps/twr_tag_tdma
newt target set twr_tag_tdma bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set twr_tag_tdma build_profile=debug
newt run twr_tag_tdma 0

```

5. On the console you should see the following expected result. 

```no-highlight

telnet localhost 19021 

....
{"utime": 23859733,"tof": 1101004815,"range": 1036004551,"azimuth": 0,"res_req":"6003EB0", "rec_tra": "6003E60"}
{"utime": 23873159,"tof": 1099956214,"range": 1034745086,"azimuth": 0,"res_req":"6004092", "rec_tra": "600403A"}
{"utime": 23879880,"tof": 1102053369,"range": 1037263961,"azimuth": 0,"res_req":"6003F78", "rec_tra": "6003F1E"}
....
{"utime": 23920210,"tof": 1104150538,"range": 1039782852,"azimuth": 0,"res_req":"6003EFB", "rec_tra": "6003E97"}
{"utime": 23926932,"tof": 1098383319,"range": 1033170778,"azimuth": 0,"res_req":"6003FE2", "rec_tra": "6003F92"}
{"utime": 23933653,"tof": 1099825167,"range": 1034587686,"azimuth": 0,"res_req":"6003ECB", "rec_tra": "6003E76"}

```


6. Making sense of the results. 

The sentense above are JSON strings. The underlying library does not support floating point printf so instead we choose to case the IEEE 754 floating-point type to a UINT32 and encapulate this within the JSON string. The decoder reverses the cast and retores the floating point representation without loss of precision.

In matlab for example:
You can use: 
```no-highlight

>> tcp = tcpclient('127.0.0.1', 19021); % to open the socket.
>> line = read(tcp); % to read the json lines
>> line = jsondecode(line); % to parse the json string
>> range = typecast(uint32(line.range,'single'); % to restore range quantity to floating point. Note all units are SI units for so the range quantity is in meters.

```

See the ./matlab/stats.m script for an example of parsing json strings.

