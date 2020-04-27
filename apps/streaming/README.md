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

The streaming example makes use of the UWB transport layer for stream data. The application uses a timer callback to enqueue packets into a memory queue asynchronously. The timer callback is called 60 times a second and attempts to enqueue 16 packets. The UWB transport is a synchronous transport scheme that makes use of the TMDA service to dequeue packets, encapsulates them in a frame header, and writes them to the tx_buffer. The TDMA scheme in this example consists of 160 slots. Within each slot, the UWB transport dequeue as many frames as the underlying transport can accommodate. The throughput depends on the data rate of the transceiver and the SPI clock frequency.

In this example 60fps x 16(buffer) x 512 (sizeof(buffer)) x 8 (bits) = 3.9Mbps are written from the Tag to the ClockMaster Node. For the uwb transport this is 160fps x 6 (frames) x 512 x  8(bits) = 3.9Mbps. The default datarate is 6.8Mbps with an 8MHz SPI clock.

The uwb transport layer coexists with other upper-mac layer services. The application layer handles the coexistence by defining how the TDMA slots are allocated. For example, by allocating every 16th slot to a nrng_request, it is possible to range to 16-nodes at 10Hz while streaming in the remaining slots.

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

### Expected Output using Interrupt backtrace

You can view the timing for the interrupts and see the occurrence of Tx frames. Below depdicts the transmission of 5 subslot frames in two slots. Each frame has a 512 byte payload with a duration of 767us (512Byte, 128Bit Preamble,SFD), there is a 300us gard between subslots resulting in 1067us for each subslot frame. The uwb_transport layer will fill each slot with subslot frames until the boundary of the next slot. In this example there are 160 slot per superframe, with a slot window of 6300us - OS_LATENCY.

```

004208 compat> dw1000 ibt
       usec   diff usec    abs usec  fctrl   status    status2txt
--------------------------------------------------------------------------------
          0           0    40792806  EF BE  000000E1  TxFrameSent|TxPHYDone|TxPreamDone|IRS
        296         296    40793102         00000011  TxStart|IRS
       1061         765    40793867  EF BE  000000E1  TxFrameSent|TxPHYDone|TxPreamDone|IRS
       2747        1686    40795553         00000011  TxStart|IRS
       3517         770    40796323  EF BE  000000E1  TxFrameSent|TxPHYDone|TxPreamDone|IRS
       3814         297    40796620         00000011  TxStart|IRS
       4586         772    40797392  EF BE  000000E1  TxFrameSent|TxPHYDone|TxPreamDone|IRS
       4882         296    40797688         00000011  TxStart|IRS
       5653         771    40798459  EF BE  000000E1  TxFrameSent|TxPHYDone|TxPreamDone|IRS
       5950         297    40798756         00000011  TxStart|IRS
       6721         771    40799527  EF BE  000000E1  TxFrameSent|TxPHYDone|TxPreamDone|IRS
       7018         297    40799824         00000011  TxStart|IRS
       7783         765    40800589  EF BE  000000E1  TxFrameSent|TxPHYDone|TxPreamDone|IRS
       9469        1686    40802275         00000011  TxStart|IRS
      10238         769    40803044  EF BE  000000E1  TxFrameSent|TxPHYDone|TxPreamDone|IRS
      10536         298    40803342         00000011  TxStart|IRS
      11308         772    40804114  EF BE  000000E1  TxFrameSent|TxPHYDone|TxPreamDone|IRS
      11604         296    40804410         00000011  TxStart|IRS
      12374         770    40805180  EF BE  000000E1  TxFrameSent|TxPHYDone|TxPreamDone|IRS
      12672         298    40805478         00000011  TxStart|IRS
      13443         771    40806249  EF BE  000000E1  TxFrameSent|TxPHYDone|TxPreamDone|IRS
      13739         296    40806545         00000011  TxStart|IRS
      14505         766    40807311  EF BE  000000E1  TxFrameSent|TxPHYDone|TxPreamDone|IRS
      16190        1685    40808996         00000011  TxStart|IRS
...
```