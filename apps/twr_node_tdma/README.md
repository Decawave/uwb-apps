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

# Decawave TDMA Example

## Overview
The twr_node_tdma and twr_tag_tdma are simple examples that showcase the TDMA features within the core library. These examples work in conjunction with the clock_mster example which established the timebase for the system. The default behavior divides the CCP_PERIOD into TDMA_NSLOTS and allocates slots to tasks. In practice the PAN/Provision services assign the system for the system, in this example, this is step omitted. The default behavior here is that each slot is consumed by the one TAG.

1. To erase the default flash image that shipped with the DWM1001 boards.

```no-highlight
$ JLinkExe -device nRF52 -speed 4000 -if SWD
J-Link>erase
J-Link>exit
$ 
```

2. On the first dwm1001-dev board build the clock_master applications. 

```no-highlight
newt target create clock_master
newt target set clock_master app=apps/clock_master
newt target set clock_master bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set clock_master build_profile=debug 
newt run clock_master 0

```

3. On 2nd dwm1001-dev board build the TDMA node (twr_node_tdma) applications for the DWM1001 module. 

```no-highlight

newt target create twr_node_tdma
newt target set twr_node_tdma app=apps/twr_node_tdma
newt target set twr_node_tdma bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set twr_node_tdma build_profile=debug
newt run twr_node_tdma 0

```

4. On 3rd dwm1001-dev board build the TDMA tag (twr_tag_tdma) applications for the DWM1001 module. 

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
>> tcp = tcpclient('127.0.0.1', 19021); % to open the socket.
>> line = read(tcp); % to read the json lines
>> line = jsondecode(line); % to parse the json string
>> range = typecast(uint32(line.range,'single'); % to restore range quantity to floating point. Note all units are SI units for so the range quantity is in meters.

See the ./matlab/stats.m script for an example of parsing json strings.

