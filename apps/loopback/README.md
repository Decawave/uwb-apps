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

# Decawave DWM1002 Loopback Example 

## Overview
The DW1002 contains two instances of the DW1000 on a single board. The board is initially intended for PDOA applicaiton, but here is use to test WCS performance. 

1. Default boot loader.

```no-highlight
newt target create dwm1002_boot
newt target set dwm1002_boot app=@apache-mynewt-core/apps/boot
newt target set dwm1002_boot bsp=@mynewt-dw1000-core/hw/bsp/dwm1002
newt target set dwm1002_boot build_profile=optimized 
newt build dwm1002_boot
newt create-image dwm1002_boot 1.0.0
newt load dwm1002_boot

```

2. On the dwm1002-dev board build the loopback example.

(executed from the mynewt-dw1000-app directory).

```no-highlight
newt target create loopback
newt target set loopback app=apps/loopback
newt target set loopback bsp=@mynewt-dw1000-core/hw/bsp/dwm1002
newt target set loopback build_profile=debug 
newt run loopback 0

newt target create listener
newt target set listener app=apps/listener
newt target set listener bsp=@mynewt-dw1000-core/hw/bsp/dwm1002
newt target set listener build_profile=debug 
newt run listener 0

```


