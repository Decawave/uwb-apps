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

# Decawave TDoA blink tag Example

## Overview

To demonstrate a simple blinking tag for tdoa example. Sleeping between blinks. 


1. To erase the default flash image that shipped with the board

```no-highlight
$ JLinkExe -device nRF52 -speed 4000 -if SWD
J-Link>erase
J-Link>exit
$ 
```

or

```
$ nrfjprog -f NRF52 -e

```

Program and build the bootloader, see other README files.

2. Build and program the application: 

```no-highlight

newt target create tdoa_tag
newt target set tdoa_tag app=apps/tdoa_tag
newt target set tdoa_tag bsp=@decawave-uwb-core/hw/bsp/dwm1001
newt target set tdoa_tag build_profile=debug
newt run tdoa_tag 0

```

