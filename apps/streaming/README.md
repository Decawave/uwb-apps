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

The streaming example makes use of the UWB transport layer for stream data. The application uses a timer callback to enqueue packets into a memory queue asynchronously. The timer callback is called 60 times a second and attempts to enqueue eight packets or less depending on mempool resources. The UWB transport is a synchronous transport scheme that makes use of the TMDA service to dequeue packets, encapsulates them in a frame header, and writes them to the tx_buffer. The TDMA scheme in this example consists of 160 slots; within each slot, the UWB transport dequeue as many frames as the underlying transport can accommodate. The throughput depends on the data rate of the transceiver and the SPI clock frequency.

In this example 60fps x 8(buffer) x 512 (sizeof(buffer)) x 8 (bits) = 1.97Mbps are written from the Tag to the ClockMaster Node. For the uwb transport  this is 160fps x 3 (frames) x (512 + 11) x  8(bits) = 2.0Mbps. The default datarate is 6.8Mbps with an 8MHz SPI clock.

The uwb transport layer coexists with other upper-mac layer services. The application layer handles the coexistence by defining how the TDMA slots are allocated. For example, by allocating every 16th slot to a nrng_request, it is possible to range to 16-nodes at 10Hz while streaming in the remaining slots. 

## Setup

1. Flash tx/rx_stream apps 

In this example the tx_stream streams transmits a buffer at 2.0Mbps to the rx_stream. The rx_stream is the designated clock master. The example also enables bleprph package so stats information can be view through AdaFruit Mynewt Manager App. 


```no-highlight

newt target create tx_stream
newt target set tx_stream app=apps/streaming
newt target set tx_stream bsp=@decawave-uwb-core/hw/bsp/dwm1001
newt target set tx_stream build_profile=debug
# Uncomment next line to use uart instead of rtt console
#newt target amend tx_stream syscfg=CONSOLE_UART_BAUD=460800:CONSOLE_UART=1:CONSOLE_RTT=0
newt target amend tx_stream syscfg=UWB_TRANSPORT_ROLE=1:OS_LATENCY=1400
newt run tx_stream 0

#newt target set tx_stream bsp=@decawave-uwb-core/hw/bsp/dwm1002
#newt run tx_stream 0

newt target create rx_stream
newt target set rx_stream app=apps/streaming
newt target set rx_stream bsp=@decawave-uwb-core/hw/bsp/dwm1001
newt target set rx_stream build_profile=debug
# Uncomment next line to use uart instead of rtt console
#newt target amend rx_stream syscfg=CONSOLE_UART_BAUD=460800:CONSOLE_UART=1:CONSOLE_RTT=0
newt target amend rx_stream syscfg=UWB_TRANSPORT_ROLE=0:OS_LATENCY=1400
newt run rx_stream 0

```

