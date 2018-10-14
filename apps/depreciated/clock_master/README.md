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

# Decawave clock_master example for clock calibration packets 

## Overview

TDOA RTLS requires a means to synchronize the anchors timestamps, and one of the ways this is achieved is using the transmission of clock calibration packets (CCP) whose TX and RX timestamps are used by the Node to track the offset between and relative drift of the clocks in the anchor sending the CCP (master anchor) and the anchors receiving the CCP (slave anchors). This information is used to correct the RX timestamps of the tag blinks to a common time base for the TDOA to solve. In spreading over a larger area where all anchors are not in range of a single master we have to have a number of masters sending CCP, and for these masters we need to arrange that their CCP transmissions do not overlap.

The CCP transmission control algorithm prevents Wireless Clock Synchronization packets from overlapping (in time) in the multi-master scalable system, so to avoid CCP collisions, the initial primary master sends CCP at the designated period and all other master anchors’ CCP transmissions should be timed from the initial master CCP or from the CCP transmissions of other subsequent (secondary) masters.

The DW1000 Device Driver model contains the control-plane for the CCP transmissions. The Clock Master is the metronome for the system, all TDMA slots are derived from the timescale that is calculated from the reception of CCP frames. 

The implementation utilises the os_timer to configure the DW1000 in advance of the required epoch and trigged a dw1000_set_delay_start()/dw1000_start_tx(). All timing is, however, relative to the DW1000 clock eliminating any OS dependencies. 

The developer needs to provide a callout dw1000_ccp_set_postprocess() that will track the offset and relative drift to establish a precise epoch for each TDMA slot. Note this can be a simple interpolation or a more advanced scheme.  

1. Build the clock_master example app.

```no-highlight

newt target create clock_master
newt target set clock_master app=apps/clock_master
newt target set clock_master bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set clock_master build_profile=debug 
newt run clock_master 0

```