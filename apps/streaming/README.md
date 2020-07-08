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

# Decawave UWB Transport Streaming Example


## Overview

The streaming example makes use of the UWB transport layer for stream data.

## Setup

1. Flash tx/rx_stream apps

In this example the tx_stream streams transmits a buffer at 3.9Mbps to the rx_stream. The rx_stream is the designated clock master. The example also enables bleprph package so stats information can be view through AdaFruit Mynewt Manager App or in the CLI.


```no-highlight

newt target create tx_stream
newt target set tx_stream app=apps/streaming
newt target set tx_stream bsp=@decawave-uwb-core/hw/bsp/dwm1001
newt target set tx_stream build_profile=debug
# Uncomment next line to use uart instead of rtt console
#newt target amend tx_stream syscfg=CONSOLE_UART_BAUD=460800:CONSOLE_UART=1:CONSOLE_RTT=0
newt target amend tx_stream syscfg=UWB_TRANSPORT_ROLE=1:OS_LATENCY=1000:CONSOLE_UART_BAUD=115200:CONSOLE_UART=1:CONSOLE_RTT=0:DW1000_SYS_STATUS_BACKTRACE_LEN=128
newt run tx_stream 0

#newt target set tx_stream bsp=@decawave-uwb-core/hw/bsp/dwm1002
#newt run tx_stream 0

newt target create rx_stream
newt target set rx_stream app=apps/streaming
newt target set rx_stream bsp=@decawave-uwb-core/hw/bsp/dwm1001
newt target set rx_stream build_profile=debug
# Uncomment next line to use uart instead of rtt console
#newt target amend rx_stream syscfg=CONSOLE_UART_BAUD=460800:CONSOLE_UART=1:CONSOLE_RTT=0
newt target amend rx_stream syscfg=UWB_TRANSPORT_ROLE=0:OS_LATENCY=1000:USE_DBLBUFFER=1:CONSOLE_UART_BAUD=115200:CONSOLE_UART=1:CONSOLE_RTT=0:DW1000_SYS_STATUS_BACKTRACE_LEN=128
newt run rx_stream 0

```

