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

# Decawave OTA over BLE Example

## Overview
The twr_node_tdma_ota_ble and twr_tag_tdma_ota_ble are simple examples that facilitates the image upgrade over air using BLE and having tdma application running parallely.

To run the below applications which are configured with UART, change the pins in hw/bsp/dwm1001/syscfg.yml

**NOTE:** Install newtmgr to perform OTA

1. To erase the default flash image that shipped with the DWM1001 boards.

```no-highlight
$ JLinkExe -device nRF52 -speed 4000 -if SWD
J-Link>erase
J-Link>exit
$ 
```

2. On 1st dwm1001-dev board build the twr_node_tdma_ota_ble applications for the DWM1001 module. 

```no-highlight

newt target create node
newt target set node app=apps/twr_node_tdma_ota_ble
newt target set node bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set node build_profile=optimized
newt target amend node syscfg="LOG_LEVEL=1"
newt build node
newt create-image node 1.0.0
newt load node

```

3. On 2nd dwm1001-dev board build the twr_tag_tdma_ota_ble applications for the DWM1001 module. 

```no-highlight

newt target create tag
newt target set tag app=apps/twr_tag_tdma_ota_ble
newt target set tag bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set tag build_profile=optimized
newt target amend tag syscfg="LOG_LEVEL=1"
newt build tag
newt create-image tag 1.0.0
newt load tag

```

4. After flashing both the devices, your console will be getting the ranging logs continuously.Now connect a bluetooth dongle to your PC if your PC has no built-in bluetooth connectivity.Then To upgrade image over air, follow the below steps:

### Create a newtmgr connection profile
```
newtmgr conn add ble type=ble connstring="peer_name=DECA-1234"   (twr_tag_tdma_ota_ble)
newtmgr conn add ble1 type=ble connstring="peer_name=DECA-4321" (twr_node_tdma_ota_ble)

```
Now that connections are being established for tag and node.

## check whether newtmgr can communicate and check image status of the devices
```
newtmgr image list -c ble
newtmgr image list -c ble1

```
With the above commands, you get the image slots of both the devices.

## Create one more image for both devices with version 2.0.0 which resides in secondary slots of tag and node.
```
newt create-image tag 2.0.0
newt create-image node 2.0.0

```
### Uploading an image to device
```
newtmgr image upload -c ble /path of the image created in bin file/
newtmgr image upload -c ble1 /path of the image created in bin file/

```
## check for image status 
```
newtmgr image list -c ble
newtmgr image list -c ble1

```
Now, you get status of primary and secondary slots with 2 different versioned images.

### Test the image
```
newtmgr image test -c ble  (hash value of secondary slot of tag)
newtmgr image test -c ble1 (hash value of secondary slot of node)

```
Now the images of secondary slot goes to pending state.
Power OFF and ON both the devices, check for image status.

## Check for image status
```
newtmgr image list -c ble
newtmgr image list -c ble1
 
```
After this, the uploaded image becomes active and will be running in primary slot.

### Confirming the image
```
newtmgr image confirm -c ble
newtmgr image confirm -c ble1

```
After this, restart minicom for the logs.
Then check for image status in the devices, see

## check for image status

You observe that images are upgraded over air and running successfully in their respective slots.
For understanding newtmgr, please have a glance at https://mynewt.apache.org/latest/tutorials/devmgmt/ota_upgrade_nrf52.html

# Changing the UWB configuration on the fly

- Connect to the console of the board (CLI), either over RTT (telnet) or uart (screen/minicom/...) and
issue commands that way. or,
- Using the ```newtmgr config``` command line tools.

## CLI: Dump the current config

To dump the current config to screen by enter the command ```config dump```:

```
005769 compat> config dump
023846 uwb/channel = 5
023846 uwb/prf = 64
023846 uwb/datarate = 6m8
023846 uwb/rx_antdly = 0x4050
023846 uwb/tx_antdly = 0x4050
023846 uwb/rx_paclen = 8
023846 uwb/rx_pream_cidx = 9
023846 uwb/rx_sfdtype = 0
023846 uwb/rx_phrmode = s
023846 uwb/tx_pream_cidx = 9
023846 uwb/tx_pream_len = 128
023846 uwb/txrf_power_coarse = 12
023846 uwb/txrf_power_fine = 18
023846 split/status = 0
...
```

## CLI: Change a config parameter

The Mynewt config package has three levels of changing parameters:

1. To change the parameter in memory but not commit to use. Several paramters can be changed and the committed at once. 
2. Commit to use but do not save to flash
3. Save to flash so these settings are preserved after reboot/power cycle

For example, to change the uwb channel to 1:

```
023847 compat> config uwb/channel 1   # Step 1 change the parameter in memory
061397 compat> config commit          # Step 2, commit changes to be used by the system
061696 Done
061696 compat> config save            # Step 3, save to flash
061939 compat> 
```
