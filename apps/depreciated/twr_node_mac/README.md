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

## mynewt-dw1000-apps for mac_filtering

## Overview 

Frame filtering allows dw1000 to receive only specific type of frames with the addresses matching in the ranging message.
The default case(without frame filtering) accepts any data and ACK frames with any destination address.

Enabled (DW1000_PAN) pan support on the tag side to receive the short_address and slot_id from the pan_master.

# Description
In this apps,

**apps/twr_tag_mac** waits for the new_ short address from the pan_master while continously sending the blink packets, without frame_filtering. After getting new short address tag enables the mac_filtering with new_short address goes into ranging mode.

**apps/pan_master** Waits for the blink packets from tag.Once it receives blink packets, it assigns short address of tag with the frame short address of pan.

**apps/twr_node_mac** It will enable the mac_filtering and start with the tag of 0XDEC1 short address. 

**NOTE:** Run tag followed by pan_master and then node.


Each twr_node_mac and twr_tag_mac are build as follows. 
(executed from the mynewt-dw1000-app directory).

### To build pan_master
```
newt target create pan
newt target set pan app=apps/pan_master
newt target set pan bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set pan build_profile=debug
```
### To run pan
```
newt run pan 0
```
### To build twr_tag_mac
```
newt target create tag_mac
newt target set tag_mac app=apps/twr_tag_mac
newt target set tag_mac bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set tag_mac build_profile=debug
```
### To run tag_mac
```
newt run tag_mac 0
```
### To create and build twr_node_mac
```
newt target create node_mac
newt target set node_mac app=apps/twr_node_mac
newt target set node_mac bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set node_mac build_profile=debug
```
### To run node_mac
```
newt run node_mac 0
```

The examples are configured to use the RTT interface. To launch the console simply telnet


telnet localhost 19021




