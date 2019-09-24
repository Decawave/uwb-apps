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

# Decawave TWR_ALOHA Example

## Overview

WIP

1. To erase the default flash image that shipped with the DWM1001 boards.

```no-highlight
$ JLinkExe -device nRF52 -speed 4000 -if SWD
J-Link>erase
J-Link>exit
$ 
```

2. On 1st dwm1001-dev board build the node application for the DWM1001 module. 

```no-highlight

newt target create twr_node
newt target set twr_node app=apps/twr_aloha
newt target set twr_node bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set twr_node build_profile=debug
newt target amend twr_node syscfg=LOG_LEVEL=1:UWBCFG_DEF_ROLE='"0x4"'
newt run twr_node 0

```

3. On 2nd dwm1001-dev board build the tag application for the DWM1001 module. 

```no-highlight

newt target create twr_tag
newt target set twr_tag app=apps/twr_aloha
newt target set twr_tag bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set twr_tag build_profile=debug
newt run twr_tag 0

```

5. On the console you should see the following expected result. 

Use telnet or nc if you are using CONSOLE_RTT: 1 in syscfg.yml
Or screen or socat if you are using CONSOLE_UART: 1 in syscfg.yml

For Human readable JSON you can use add FLOAT_USER
```no-highlight
newt target amend twr_node syscfg=FLOAT_USER=1
```

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

7. Trying different ranging algorithms

The method for ranging used is selected by including the corresponding package in the pkg.yml file.
Note that only one of the twr_xx packages should be included at any one point in time. For instance,
to use the double sided two way ranging change the pkg.yml as below:

```
<excerpt from pkg.yml>
...
    - "@apache-mynewt-core/encoding/json"
    - "@mynewt-dw1000-core/hw/drivers/uwb"
# Use only one of the twr pkgs below
#    - "@mynewt-dw1000-core/lib/twr_ss"
#    - "@mynewt-dw1000-core/lib/twr_ss_ext"
     - "@mynewt-dw1000-core/lib/twr_ds"
#    - "@mynewt-dw1000-core/lib/twr_ds_ext"
    - "@mynewt-dw1000-core/sys/uwbcfg"
    - "@apache-mynewt-core/boot/split"

```
