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

# Decawave Desense RF Test Example

## Overview

Application for performing the three roles of Desense testing:
1. Tag, responds to request for test packets
2. Aggressor, continously sends packets to interfere
3. DUT - Device under test, instructs the tag to send data and collects statistics


### Setup of boards

Works on any of the boards with a bsp under decawave-uwb-core/hw/bsp. Examples below
given for the DW1000 based dwm1001.

1. Erase the boards

```no-highlight
# Nordic
$ JLinkExe -device nRF52 -speed 4000 -if SWD
J-Link>erase
J-Link>exit
$
# For STM32:
$ st-flash erase
```

2. Build and load bootloader

see other readmes

3. Build and Load Desense application

For other bsps just replace the bsp variable below. All roles use the same application.

```no-highlight
newt target create dwm1001_desense
newt target set dwm1001_desense app=apps/desense
newt target set dwm1001_desense bsp=@decawave-uwb-core/hw/bsp/dwm1001
newt target set dwm1001_desense build_profile=debug
newt run dwm1001_desense 0 --extrajtagcmd '-select usb=<serial of respective jtag>'
```

## Run a test

1. Connect to the uart console

Use screen, socat, minicom or similar. The parameters are 115200-8-n-1.

2. On Tag console, issue:

```
108974 compat> desense rx
109494 Listening on 0x0CA8
109494 compat>
```

Note the address given, in this case 0x0CA8. This command has to be issued before
each test is started. 

3. On the Aggressor console:

```
# Set a different preamble to not be accepted as real packets by DUT:
104572 compat> config uwb/tx_pream_cidx 11
104743 compat> config commit
104756 compat>
# Activate repeated TX:
# desense txon <instance> <packet data length> <start to start interval in ns>
119496 compat> desense txon 0 32 1000000

# To list other config parameters and change them use the config commands:
062141 compat> config dump
062453 uwb/channel = 5
062453 uwb/prf = 64
062453 uwb/datarate = 6m8
062453 uwb/rx_paclen = 8
062453 uwb/rx_pream_cidx = 9
062453 uwb/rx_sfdtype = 1
062453 uwb/rx_phrmode = e
062453 uwb/rx_diag_en = 0
062453 uwb/tx_pream_cidx = 9
062453 uwb/tx_pream_len = 128
062453 uwb/txrf_power_coarse = 18
062453 uwb/txrf_power_fine = 31
062454 uwb/rx_antdly = 0x4050
062455 uwb/tx_antdly = 0x4050
062455 uwb/rx_ant_separation = 0.0205
062455 uwb/ext_clkdly = 0
062456 uwb/role = 0
062456 uwb/frame_filter = 0
062456 uwb/xtal_trim = 0xff
062457 compat>

# In order to make the config parameters active they have to be commited with config commit
# To make them persistent between resets use config save
```

To stop the aggressor the ```reset``` command is easiest.

On the DUT console start the test by issuing the desense req <addr> command:

```
014417 compat> desense req 0xca8
031232 request sent to 0x0ca8
...
# Packets received:
{"idx": 0, "nth_pkt": 0, "rssi": -85.879, "clko_ppm": -0.328398}
{"idx": 1, "nth_pkt": 1, "rssi": -85.681, "clko_ppm": -0.382845}
...
{"idx": 99, "nth_pkt": 99, "rssi": -85.274, "clko_ppm": -0.827587}

# Event counters:
  RXPHE:       0  # rx PHR err
  RXFSL:       0  # rx sync loss
  RXFCG:     101  # rx CRC OK
  RXFCE:       0  # rx CRC Errors
  ARFE:        0  # addr filt err
  RXOVRR:      0  # rx overflow
  RXSTO:       0  # rx SFD TO
  RXPTO:       0  # pream search TO
  FWTO:       14  # rx frame wait TO
  TXFRS:       0  # tx frames sent

  PER:  0.000000
  RSSI: -85.413
  CLKO: -0.576778 ppm
```

To configure the test parameters use the ```desense set``` command like so:

```
# To list parameters and their values:
030602 compat> desense set
030892 Desense RF Test parameters:
030892   ms_start_delay        1000    # Delay between START packets and first TEST packet
030892   ms_test_delay           10    # Delay between TEST packets
030892   strong_coarse_power      9    # Coarse tx power used for START and END packets
030892   strong_fine_power       31    # Fine tx power used for START and END packets
030892   n_strong                 5    # Number of START and END packets
030892   test_coarse_power        3    # Coarse tx power used for TEST packets
030892   test_fine_power          9    # Fine tx power used for TEST packets
030892   n_test                 100    # Number of TEST packets
030893 compat>

# To change the value of a paramter:
030893 compat> desense set n_test 1234
049589 Desense RF Test parameters:
049589   ms_start_delay        1000
049589   ms_test_delay           10
049589   strong_coarse_power      9
049589   strong_fine_power       31
049589   n_strong                 5
049589   test_coarse_power        3
049589   test_fine_power          9
049589   n_test                1234

# Test parameters are not persistent between resets
```

## Manipulating the interal antenna mux

DW3000's antenna mux can be manipulated using the cli.

- Force ant 1, pdoa start on ant 1:  0x1100
- Force ant 2, pdoa start on ant 2:  0x2102
- Force ant 1, no ant-sw in pdoa_md: 0x1101
- Force ant 2, no ant-sw in pdoa_md: 0x2103
- Default: 0x0000

```
# Read out the current antenna mux register value:
045700 compat> dw3000 rd 0 0x70014 0 4
050625 0x070014,0x0000: 0x1C000000

# Set mux to only use ant 2 from above:
050626 compat> dw3000 wr 0 0x70014 0 0x2103 4
148045 0x070014,0x0 w: 0x2103
```

The antenna mux is at register 0x70014, offset 0, length 4 bytes.


## Relevant source code

```
apps/uwb_desense
├── Kbuild
├── pkg.yml
├── README.md
└── src
    └── uwb_desense.c
lib/uwb_rf_tests/
└── desense
    ├── CMakeLists.txt
    ├── include
    │   └── desense
    │       └── desense.h
    ├── pkg.yml
    ├── src
    │   ├── desense.c
    │   ├── desense_cli.c
    │   ├── desense_debugfs.c
    │   └── desense_sysfs.c
    └── syscfg.yml
```