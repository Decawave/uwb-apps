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

# Decawave DW1000 Application lwip_ping_rx

## Overview
The Decawave DW1000 Application lwip_ping_rx showcases the ability of lwIP driver to send and receive 
payloads to and from a node. In this sample application, we are receiving a ping from Node A.

## Pre-Requisites
Repo 	:	mynewt-dw1000-apps
Branch	:	refactor

Repo	:	mynewt-dw1000-core
Branch	:	refactor

Repo	:	apache-mynewt-core
Branch	:	master
Tag 	:	1.4.1

## Building
1. Build and flash the lwip_ping_rx app.

```no-highlight
newt target create lwip_ping_rx
newt target set lwip_ping_rx app=apps/lwip_ping_rx
newt target set lwip_ping_rx bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set lwip_ping_rx build_profile=debug
newt build lwip_ping_rx
newt create-image lwip_ping_rx 1.0.0
newt load lwip_ping_rx
```

## Testing
1. Flash lwip_ping_tx into Node A.
2. Build and flash lwip_ping_rx in Node B.
3. Connect both the nodes to Dev PC and monitor the logs of the nodes in serial comm application.
4. The sample output is as shown below,
	On Node B:

```no-highlight
[Ping Received]
Seq # - 1234
```
