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

# Decawave Listener Example 
Listens to all uwb traffic and outputs json to uart:

```
# Example with two receivers
{"ts":418793104067,"utime":92619018,"rssi0":"-90.9","rssi1":"-94.4","pd":"1.374","dlen":10,"d":"c5cbf01c0010c2bf4020"}
{"ts":419282763488,"utime":92626678,"rssi0":"-90.9","rssi1":"-94.4","pd":"0.305","dlen":10,"d":"c5ccf01c0010c2bf4020"}
{"ts":419784023582,"utime":92634521,"rssi0":"-91.1","rssi1":"-94.0","pd":"0.014","dlen":10,"d":"c5cdf01c0010c2bf4020"}
{"ts":420283186433,"utime":92642333,"rssi0":"-90.8","rssi1":"-93.9","pd":"0.234","dlen":10,"d":"c5cef01c0010c2bf4020"}
```

## Overview

# DWM1001  

```
newt target create listener
newt target set listener app=apps/listener
newt target set listener bsp=@mynewt-dw1000-core/hw/bsp/dwm1003
newt target set listener build_profile=debug 
newt run listener 0
```

# dwm1001_ncbwifi

```
newt target create dwm1001_ncbwifi_listener
newt target set dwm1001_ncbwifi_listener app=apps/listener
newt target set dwm1001_ncbwifi_listener bsp=@mynewt-dw1000-core/hw/bsp/dwm1001_ncbwifi
newt target set dwm1001_ncbwifi_listener build_profile=debug
newt target amend dwm1001_ncbwifi_listener syscfg=CONSOLE_UART_BAUD=115200
newt run dwm1001_ncbwifi_listener 0
```

# lps2nano_ncbwifi

```
newt target create lps2nano_ncbwifi_listener
newt target set lps2nano_ncbwifi_listener app=apps/listener
newt target set lps2nano_ncbwifi_listener bsp=@mynewt-dw1000-core/hw/bsp/lps2nano_ncbwifi
newt target set lps2nano_ncbwifi_listener build_profile=debug
newt target amend lps2nano_ncbwifi_listener syscfg=CONSOLE_UART_RX_BUF_SIZE=256:ESP12F_ENABLED=1:SHELL_NEWTMGR=1:CONSOLE_ECHO=0:NCBWIFI_ESP_PASSTHROUGH=1
newt run lps2nano_ncbwifi_listener 0
```

# dwm1002_devkit

```
newt target create dwm1002_listener
newt target set dwm1002_listener app=apps/listener
newt target set dwm1002_listener bsp=@mynewt-dw1000-core/hw/bsp/dwm1002
newt target set dwm1002_listener build_profile=debug
newt target amend dwm1002_listener syscfg=CONSOLE_UART_BAUD=460800:DW1000_DEVICE_0=1:DW1000_DEVICE_1=1
newt run dwm1002_listener 0
```


