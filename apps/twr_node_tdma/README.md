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

# Decawave TWR_TDMA Example

## Overview

The twr_node_tdma and twr_tag_tdma illustrate the capabilities of the underlying decawave-uwb-core library. The default behavior divides the TDMA_PERIOD into TDMA_NSLOTS and allocates slots to a single ranging task. Clock calibration and timescale compensation are enabled within the twr_tag_tdma application; this has the advantage of permitting single-sided two range exchange thus reducing the channel utilization. 

Another advantege of wireless synchronization is the ability to turn-on transeiver is a just-in-time fashion thereby reducing the power comsumption. In these examples transeiver is turned on for approximatly 190 usec per each slot. 

### Under-the-hood

The decawave-uwb-core driver implements the MAC layers and exports a MAC extension interface for additional services. One such service is ranging (./lib/rng). The ranging services through the extension interface expose callback to various events within the ranging process. One such callback is the complete_cb which marks the successful completion of the range request. In these examples, we attach to the complete_cb and perform subsequent processing. The available callbacks are defined in the struct uwb_mac_interface which is defined in uwb.h

This example also illustrates the clock calibration packet (CCP) and time division multiple access (TDMA) services. Both of which also bind to the MAC interface. In a synchronous network, the CCP service establishes the metronome through superframes transmissions. All epochs are derived from these superframes. The TDMA service divides the superframe period into slots and schedules event about each slot. The transceiver controls the precise timing of all frames to the microsecond. The operating system loosely schedules an event in advance of the desired epoch and this event issues a delay_start request to the transceiver. This loose/tight timing relationship reduces the timing requirements on the OS and permits the dw1000 to operate at optimum efficiency.

1. To erase the default flash image that shipped with the DWM1001 boards.

```no-highlight
$ JLinkExe -device nRF52 -speed 4000 -if SWD
J-Link>erase
J-Link>exit
$ 
```

2. On 1st dwm1001-dev board build the TDMA node (twr_node_tdma) applications for the DWM1001 module. 

```no-highlight

newt target create twr_node_tdma
newt target set twr_node_tdma app=apps/twr_node_tdma
newt target set twr_node_tdma bsp=@decawave-uwb-core/hw/bsp/dwm1001
newt target set twr_node_tdma build_profile=debug
newt target amend twr_node_tdma syscfg=LOG_LEVEL=1:UWBCFG_DEF_ROLE='"0x1"':BLE_ENABLED=1
# Uncomment next line to use uart instead of rtt console
#newt target amend twr_node_tdma syscfg=CONSOLE_UART_BAUD=460800:CONSOLE_UART=1:CONSOLE_RTT=0:UWBCFG_DEF_FRAME_FILTER='"0x00F"'
newt run twr_node_tdma 0

```

3. On 2nd dwm1001-dev board build the TDMA tag (twr_tag_tdma) applications for the DWM1001 module. 

```no-highlight

newt target create twr_tag_tdma
newt target set twr_tag_tdma app=apps/twr_tag_tdma
newt target set twr_tag_tdma bsp=@decawave-uwb-core/hw/bsp/dwm1001
newt target set twr_tag_tdma build_profile=debug
newt target amend twr_node_tdma syscfg=BLE_ENABLED=1
# Uncomment next line to use uart instead of rtt console
#newt target amend twr_tag_tdma syscfg=CONSOLE_UART_BAUD=460800:CONSOLE_UART=1:CONSOLE_RTT=0
newt run twr_tag_tdma 0

```

5. On the console you should see the following expected result. 

```no-highlight

{"utime": 9484006864249, "wcs": [9483617024080,687524001872,8752525324842,687524001872], "skew": 13752215634709839872}
{"utime": 9484254696807, "twr": {"rng": "1.071","uid": "d22d"},"uid": "55a6", "diag": {"rssi": "-79.562","los": "1.000"}}
{"utime": 9484684610480, "twr": {"rng": "1.061","uid": "d22d"},"uid": "55a6", "diag": {"rssi": "-79.493","los": "1.000"}}
{"utime": 9485113714163, "twr": {"rng": "1.064","uid": "d22d"},"uid": "55a6", "diag": {"rssi": "-79.416","los": "1.000"}}
{"utime": 9485543643708, "twr": {"rng": "1.079","uid": "d22d"},"uid": "55a6", "diag": {"rssi": "-79.593","los": "1.000"}}
{"utime": 9485972719232, "twr": {"rng": "1.082","uid": "d22d"},"uid": "55a6", "diag": {"rssi": "-79.653","los": "1.000"}}


```

6. For PDOA on DWM1002 module.

```no-highlight

newt target create dwm1002_twr_node_tdma
newt target set dwm1002_twr_node_tdma app=apps/twr_node_tdma
newt target set dwm1002_twr_node_tdma bsp=@decawave-uwb-core/hw/bsp/dwm1002
newt target set dwm1002_twr_node_tdma build_profile=debug
newt target amend dwm1002_twr_node_tdma syscfg=UWB_DEVICE_0=1:UWB_DEVICE_1=1:USE_DBLBUFFER=0:CIR_ENABLED=1:LOG_LEVEL=1:UWBCFG_DEF_ROLE='"0x1"'
# Uncomment next line to use uart instead of rtt console
#newt target amend dwm1002_twr_node_tdma syscfg=CONSOLE_UART=1:CONSOLE_RTT=0
newt run dwm1002_twr_node_tdma 0

newt target create dwm1003_twr_tag_tdma
newt target set dwm1003_twr_tag_tdma app=apps/twr_tag_tdma
newt target set dwm1003_twr_tag_tdma bsp=@decawave-uwb-core/hw/bsp/dwm1003
newt target set dwm1003_twr_tag_tdma build_profile=debug
# Uncomment next line to use uart instead of rtt console
#newt target amend dwm1003_twr_tag_tdma syscfg=CONSOLE_UART=1:CONSOLE_RTT=0
newt run dwm1003_twr_tag_tdma 0


```

7. On the console you should see the following expected result.

```no-highlight

{"utime": 15046841179648, "twr": {"raz": ["5.397","1.662","null"],"uid": "56a5"},"uid": "4dad", "diag": {"rssi": "-79.178","los": "1.000"}}
{"utime": 15047270679552, "twr": {"raz": ["5.388","1.798","null"],"uid": "56a5"},"uid": "4dad", "diag": {"rssi": "-79.644","los": "1.000"}}
{"utime": 15047699727872, "twr": {"raz": ["5.408","1.565","null"],"uid": "56a5"},"uid": "4dad", "diag": {"rssi": "-79.651","los": "1.000"}}
{"utime": 15048129227264, "twr": {"raz": ["5.430","1.390","null"],"uid": "56a5"},"uid": "4dad", "diag": {"rssi": "-79.516","los": "1.000"}}
{"utime": 15048558718976, "twr": {"raz": ["5.381","1.075","null"],"uid": "56a5"},"uid": "4dad", "diag": {"rssi": "-78.911","los": "1.000"}}
{"utime": 15048988214272, "twr": {"raz": ["5.402","1.595","null"],"uid": "56a5"},"uid": "4dad", "diag": {"rssi": "-79.438","los": "1.000"}}
{"utime": 15049417705984, "twr": {"raz": ["5.412","1.637","null"],"uid": "56a5"},"uid": "4dad", "diag": {"rssi": "-79.239","los": "1.000"}}
{"utime": 15049847641088, "twr": {"raz": ["5.440","1.924","null"],"uid": "56a5"},"uid": "4dad", "diag": {"rssi": "-79.339","los": "1.000"}}


```

8. For PDOA with DW3000 module on STM.

```no-highlight

# Erase flash
#sudo apt-get install stlink-tools
st-flash erase

# Bootloader (used for both tag and node)
newt target create nucleo-f429zi_boot
newt target set nucleo-f429zi_boot app=@mcuboot/boot/mynewt
newt target set nucleo-f429zi_boot bsp=@decawave-uwb-core/hw/bsp/nucleo-f429zi
newt target set nucleo-f429zi_boot build_profile=optimized
newt build nucleo-f429zi_boot
newt load nucleo-f429zi_boot

# Node application
newt target create nucleo-f429zi_twr_node_tdma
newt target set nucleo-f429zi_twr_node_tdma app=apps/twr_node_tdma
newt target set nucleo-f429zi_twr_node_tdma bsp=@decawave-uwb-core/hw/bsp/nucleo-f429zi
newt target set nucleo-f429zi_twr_node_tdma build_profile=debug
newt target amend nucleo-f429zi_twr_node_tdma syscfg=UWB_DEVICE_0=1:USE_DBLBUFFER=0:LOG_LEVEL=1:UWBCFG_DEF_ROLE='"0x1"'
newt target amend nucleo-f429zi_twr_node_tdma syscfg=UWBCFG_DEF_RX_PDOA_MODE='"3"':UWBCFG_DEF_RX_STS_MODE='"1sdc"':UWBCFG_DEF_TX_PREAM_LEN='"64"':UWBCFG_DEF_RX_SFD_TYPE='"1"':UWBCFG_DEF_RX_CIPHER_LEN='"256"':UWBCFG_DEF_FRAME_FILTER='"0xF"'
# If the config seems "stuck" you can force it to change with the line below. You may need a different (random) value at the end
newt target amend nucleo-f429zi_twr_node_tdma syscfg=CONFIG_FCB_MAGIC=0x12345678
# Uncomment next line to use uart instead of rtt console
newt target amend nucleo-f429zi_twr_node_tdma syscfg=CONSOLE_UART=1:CONSOLE_UART_BAUD=115200:CONSOLE_RTT=0
# Uncomment next line if the aoa angle appears inverted (Antenna facing other way)
#newt target amend nucleo-f429zi_twr_node_tdma syscfg=AOA_ANGLE_INVERT=1
# Uncomment next line to enable the RXTX GPIO masks.
newt target amend nucleo-f429zi_twr_node_tdma syscfg=DW3000_RXTX_GPIO=1
newt run nucleo-f429zi_twr_node_tdma 0

# Tag application
newt target create nucleo-f429zi_twr_tag_tdma
newt target set nucleo-f429zi_twr_tag_tdma app=apps/twr_tag_tdma
newt target set nucleo-f429zi_twr_tag_tdma bsp=@decawave-uwb-core/hw/bsp/nucleo-f429zi
newt target set nucleo-f429zi_twr_tag_tdma build_profile=debug
newt target amend nucleo-f429zi_twr_tag_tdma syscfg=UWBCFG_DEF_RX_STS_MODE='"1sdc"':UWBCFG_DEF_TX_PREAM_LEN='"64"':UWBCFG_DEF_RX_SFD_TYPE='"1"':UWBCFG_DEF_RX_STS_LEN='"256"'
# If the config seems "stuck" you can force it to change with the line below. You may need a different (random) value at the end
newt target amend nucleo-f429zi_twr_tag_tdma syscfg=CONFIG_FCB_MAGIC=0x12345678
# Uncomment next line to use uart instead of rtt console
newt target amend nucleo-f429zi_twr_tag_tdma syscfg=CONSOLE_UART=1:CONSOLE_UART_BAUD=115200:CONSOLE_RTT=0
newt run nucleo-f429zi_twr_tag_tdma 0

# On mac
minicom -D /dev/tty.usbmodem* -b 115200

```

Note for pdoa on the dw3000 to work (in this example) the cipher mode must be "1sdc",
preamble_length should be "64", and the cipher preamble length should be 256. The node should also have pdoa_mode
set to "3" for best accuracy (pdoa_mode "1" works too, but is less accurate).

9. For non-PDOA on DW3000 module on STM.

```no-highlight

# Erase flash
#sudo apt-get install stlink-tools
st-flash erase

# Bootloader (used for both tag and node)
newt target create nucleo-f429zi_boot
newt target set nucleo-f429zi_boot app=@mcuboot/boot/mynewt
newt target set nucleo-f429zi_boot bsp=@decawave-uwb-core/hw/bsp/nucleo-f429zi
newt target set nucleo-f429zi_boot build_profile=optimized
newt build nucleo-f429zi_boot
newt load nucleo-f429zi_boot

# Node application
newt target create nucleo-f429zi_twr_node_tdma
newt target set nucleo-f429zi_twr_node_tdma app=apps/twr_node_tdma
newt target set nucleo-f429zi_twr_node_tdma bsp=@decawave-uwb-core/hw/bsp/nucleo-f429zi
newt target set nucleo-f429zi_twr_node_tdma build_profile=debug
newt target amend nucleo-f429zi_twr_node_tdma syscfg=UWB_DEVICE_0=1:USE_DBLBUFFER=0:LOG_LEVEL=1:UWBCFG_DEF_ROLE='"0x1"'
newt target amend nucleo-f429zi_twr_node_tdma syscfg=CONSOLE_UART=1:CONSOLE_UART_BAUD=115200:CONSOLE_RTT=0
newt run nucleo-f429zi_twr_node_tdma 0

# Tag application
newt target create nucleo-f429zi_twr_tag_tdma
newt target set nucleo-f429zi_twr_tag_tdma app=apps/twr_tag_tdma
newt target set nucleo-f429zi_twr_tag_tdma bsp=@decawave-uwb-core/hw/bsp/nucleo-f429zi
newt target set nucleo-f429zi_twr_tag_tdma build_profile=debug
newt target amend nucleo-f429zi_twr_tag_tdma syscfg=CONSOLE_UART=1:CONSOLE_UART_BAUD=115200:CONSOLE_RTT=0
newt run nucleo-f429zi_twr_tag_tdma 0

# On mac
minicom -D /dev/tty.usbmodem* -b 115200

```

9. For PDOA with DWM3020 on nRF52.

```no-highlight

# Bootloader (used for both tag and node)
newt target create pca10056_dw3000_boot
newt target set pca10056_dw3000_boot app=@mcuboot/boot/mynewt
newt target set pca10056_dw3000_boot bsp=@decawave-uwb-core/hw/bsp/pca10056_dw3000
newt target set pca10056_dw3000_boot build_profile=optimized
newt build pca10056_dw3000_boot
newt load pca10056_dw3000_boot

# Node application
newt target create pca10056_dw3000_twr_node_tdma
newt target set pca10056_dw3000_twr_node_tdma app=apps/twr_node_tdma
newt target set pca10056_dw3000_twr_node_tdma bsp=@decawave-uwb-core/hw/bsp/pca10056_dw3000
newt target set pca10056_dw3000_twr_node_tdma build_profile=debug
newt target amend pca10056_dw3000_twr_node_tdma syscfg=UWB_DEVICE_0=1:USE_DBLBUFFER=0:LOG_LEVEL=1:UWBCFG_DEF_ROLE='"0x1"'
#newt target amend pca10056_dw3000_twr_node_tdma syscfg=UWBCFG_DEF_RX_PDOA_MODE='"3"':UWBCFG_DEF_RX_STS_MODE='"0"':UWBCFG_DEF_TX_PREAM_LEN='"64"'
# Uncomment next line to enable PDOA
newt target amend pca10056_dw3000_twr_node_tdma syscfg=UWBCFG_DEF_RX_PDOA_MODE='"3"':UWBCFG_DEF_RX_STS_MODE='"1sdc"':UWBCFG_DEF_TX_PREAM_LEN='"64"':UWBCFG_DEF_RX_SFD_TYPE='"1"':UWBCFG_DEF_RX_STS_LEN='"256"':UWBCFG_DEF_FRAME_FILTER='"0xF"'
# Uncomment next line to use uart instead of rtt console
newt target amend pca10056_dw3000_twr_node_tdma syscfg=CONSOLE_UART_BAUD=115200:CONSOLE_UART=1:CONSOLE_RTT=0
# Uncomment next line if the aoa angle appears inverted (Antenna facing other way)
#newt target amend pca10056_dw3000_twr_node_tdma syscfg=AOA_ANGLE_INVERT=1
# Uncomment next line to enable the RXTX GPIO masks.
newt target amend pca10056_dw3000_twr_node_tdma syscfg=DW3000_RXTX_GPIO=1
newt target amend pca10056_dw3000_twr_node_tdma syscfg=RNG_VERBOSE=0
newt run pca10056_dw3000_twr_node_tdma 0

# Tag application
newt target create pca10056_dw3000_twr_tag_tdma
newt target set pca10056_dw3000_twr_tag_tdma app=apps/twr_tag_tdma
newt target set pca10056_dw3000_twr_tag_tdma bsp=@decawave-uwb-core/hw/bsp/pca10056_dw3000
newt target set pca10056_dw3000_twr_tag_tdma build_profile=debug
#newt target amend pca10056_dw3000_twr_tag_tdma syscfg=UWBCFG_DEF_RX_STS_MODE='"0"':UWBCFG_DEF_TX_PREAM_LEN='"64"'
# Uncomment next line to enable PDOA
newt target amend pca10056_dw3000_twr_tag_tdma syscfg=UWBCFG_DEF_RX_STS_MODE='"1sdc"':UWBCFG_DEF_TX_PREAM_LEN='"64"':UWBCFG_DEF_RX_SFD_TYPE='"1"':UWBCFG_DEF_RX_STS_LEN='"256"'
# Uncomment next line to use uart instead of rtt console
newt target amend pca10056_dw3000_twr_tag_tdma syscfg=CONSOLE_UART_BAUD=115200:CONSOLE_UART=1:CONSOLE_RTT=0
newt target amend pca10056_dw3000_twr_tag_tdma syscfg=DW3000_RXTX_GPIO=1
newt target amend pca10056_dw3000_twr_tag_tdma syscfg=RNG_VERBOSE=0
newt run pca10056_dw3000_twr_tag_tdma 0

# On mac
minicom -D /dev/tty.usbmodem* -b 115200

```

9. Using TWR-SS-ACK range model on a board with long/irregular interrupt latency

The TWR-SS-ACK range model uses the hardware auto-ack capability to speed up the
initial respons to a request. A follow up frame is then sent with the tx-time
of the ack itself.
Auto-ack requires that the frame-filter is enabled:

```no-highlight
newt target amend <target_name> syscfg=UWBCFG_DEF_FRAME_FILTER='"0xF"'

```

10. Viewing the CIR Accumulator data

The accumulator is used to timestamp the incoming message by analysing the preamble received.
This data can be printed to the console by adding the following syscfg options:
```
newt target amend <target> syscfg=CIR_ENABLED=1:CIR_VERBOSE=1:CIR_OFFSET=8:CIR_SIZE=16
```

Interleaved with the range results you should now also get lines like the one below:

```no-highlight

{"utime": 657021372,"cir": {"idx": 1144608512,"power": 3261357632,"real": [0,0,0,0,1,0,0,0,0,0,11,27,24,-11,-24,-2],"imag": [0,0,0,0,0,0,0,-2,-14,-47,-48,-42,6,45,40,8]}}
{"utime": 657270997,"cir": {"idx": 1144516096,"power": 3261436783,"real": [0,0,0,0,0,1,0,-3,-30,-32,-20,11,33,15,7,7],"imag": [0,43,14,41,-44,-29,-34,14,273,860,544,-223,-1289,-444,343,12]}}
```

where the real and imaginary (R and Q) vectors surrounding the leading edge. CIR_SIZE and CIR_OFFSET controls the length and where
the leading edge is placed in the vector respectively. Note that these values are normalised with the number of accumulated
symbols to be comparable between different preamble lengths.

11. Changing the configuration without recompiling the code

The mynewt os offers a set of console config tools. To see what config parameters are available use the ```config dump``` command:

```
1786287 compat> config dump
1788709 uwb/channel = 5
1788709 uwb/datarate = 6m8
1788709 uwb/rx_paclen = 8
1788709 uwb/rx_pream_cidx = 9
1788710 uwb/rx_sfdtype = 1
1788710 uwb/rx_phrmode = e
1788710 uwb/rx_sts_mode = 1sdc
1788710 uwb/rx_sts_len = 256
1788710 uwb/rx_pdoa_mode = 3
1788710 uwb/tx_pream_cidx = 9
1788712 uwb/tx_pream_len = 64
1788712 uwb/txrf_power_coarse = 3
1788713 uwb/txrf_power_fine = 31
1788714 uwb/txrf_power_vcm_lo = 15
1788714 uwb/rx_antdly = 0x4050
1788715 uwb/tx_antdly = 0x4050
1788716 uwb/ext_clkdly = 0
1788716 uwb/role = 0x1
1788717 uwb/frame_filter = 0xF
```

For example, to disable the cipher_mode so that DW3000 can interoperate with DW1000 do the following:

```
> config uwb/rx_cipher_mode 0   # change parameter "uwb/rx_cipher_mode" to the value "0"
> config commit                 # make this change active
> config save                   # save to flash so this new change is used even after reset
```

### Visualisation

If you have node-red installed you can import the flow in the node-red folder. Note that
it uses RTT to connect to the dwm1002 board so you have to be still running the
```newt run twr_node_tdma 0``` command on the same computer for it to work.

```
# Install nodejs and node-red
sudo apt-get install -y nodejs
sudo npm install -g --unsafe-perm node-red
# Once installed, run node-red on the commandline:
node-red
```

The palette of node-red modules needs to be updated to have
**node-red-dashboard** installed:

1. In the top right corner menu select **Manage palette**
2. Click on the **Install** tab
3. Search for and install **node-red-dashboard**

Once the prerequisites are setup, in node-red, open up the menu in the top right corner, select import,
clipboard, and select file to import.
Navigate to the node-red directory and select the pdoa_viewer.json file. Or, simply drag and drop the file
on the node-red webpage.
Select deploy. Done.




