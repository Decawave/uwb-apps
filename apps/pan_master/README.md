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

# Decawave pan_master example for discovery

## Overview

1. Build the PAN examples app.

```no-highlight

newt target create pan_master
newt target set pan_master app=apps/pan_master
newt target set pan_master bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set pan_master build_profile=debug 
newt run pan_master 0

newt target create pan_tag
newt target set pan_tag app=apps/pan_tag
newt target set pan_tag bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set pan_tag build_profile=debug 
newt run pan_tag 0

newt target create pan_node
newt target set pan_node app=apps/pan_node
newt target set pan_node bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set pan_node build_profile=debug 
newt run pan_node 0

```

