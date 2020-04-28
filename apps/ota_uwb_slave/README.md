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
```no-highlight

The ota_uwb_master & ota_uwb_slave are simple examples that facilitates the image upgrade over air using UWB with a CLI.

To run the below applications which are configured with UART, change the pins in hw/bsp/dwm1001/syscfg.yml

1. To erase the default flash image that shipped with the DWM1001 boards.

$ JLinkExe -device nRF52 -speed 4000 -if SWD
J-Link>erase
J-Link>exit
$
```
```no-highlight
2. On 1st dwm1001-dev board build the ota_uwb_master applications for the DWM1001 module.

newt target create master
newt target set master app=apps/ota_uwb_master
newt target set master bsp=@decawave-uwb-core/hw/bsp/dwm1001
newt target set master build_profile=optimized
newt target amend master syscfg="LOG_LEVEL=1"
newt build master
newt create-image master 1.0.0
newt load master
```
```no-highlight
3. On 2nd dwm1001-dev board build the ota_uwb_slave applications for the DWM1001 module.

newt target create slave
newt target set slave app=apps/ota_uwb_slave
newt target set slave bsp=@decawave-uwb-core/hw/bsp/dwm1001
newt target set slave build_profile=optimized
newt target amend slave syscfg="LOG_LEVEL=1"
newt build slave
newt create-image slave 1.0.0
newt load slave
```

#i## How to test

1. Upload the image that needs to be sent to slave node using BLE to the master node

### Create a newtmgr connection profile
```
newtmgr conn add ble type=ble connstring="peer_name=DECA-4321"   (ota_uwb_master)

```
## check whether newtmgr can communicate and check image status of the device
```
newtmgr image list -c ble

```
With the above command, you get the image slots of the device

## Create a the image of interest
```
newt create-image slave 2.0.0
```
### Uploading an image to device
```
newtmgr image upload -c ble /path of the image created in bin file/

```
## check for image status
```
newtmgr image list -c ble

```
Now, you get status of primary and secondary slots with 2 different versioned images.

2. Once the required image has been uploaded in the second slot start the upload of this image to slave node
### check whether master can talk to slave
```
img_list 0x1234 (Slave node address)
Should get some logs like this
slot = 0
Version = 0.0.0
bootable : true
Flags : active confirmed
7b6af13ca48f82fe22e009393779a3100458c2dc99e4e86ea38922c0aec5380a

With the above commands, you get the image slots of both the devices.
```
### Uploading an image to device
```
img_upload slot2 0x1234 (Slave node address)

```
### check for image status
```
img_list 0x1234 (Slave node address)
Should get some logs like this
Slot = 0
Version = 1.0.0
bootable : true
Flags : active confirmed
6479df5a957bf7fa2e9b45e075788fd91e3fb15895affee06ff3b9cb4e546645

Slot = 1
Version = 1.0.0
bootable : true
Flags :
76609ae104fd47f74413d1c39501569605a12c2d17f17922cc3e417448244ab7
```
Now, you get status of primary and secondary slots with 2 different versioned images.
### Test the image
```
img_set_state test 76609ae104fd47f74413d1c39501569605a12c2d17f17922cc3e417448244ab7(32 byte hash of testing image) 0x1234(Slave node address)
Should get some logs like this
Slot = 0
Version = 1.0.0
bootable : true
Flags : active confirmed
6479df5a957bf7fa2e9b45e075788fd91e3fb15895affee06ff3b9cb4e546645

Slot = 1
Version = 1.0.0
bootable : true
Flags : pending
76609ae104fd47f74413d1c39501569605a12c2d17f17922cc3e417448244ab7
```
Now the images of secondary slot goes to pending state.

**NOTE:** Then power OFF and ON the slave node.

Once the device boots up
## Check for image status
```
img_list 0x1234 (Slave node address)
Slot = 0
Version = 1.0.0
bootable : true
Flags : active
76609ae104fd47f74413d1c39501569605a12c2d17f17922cc3e417448244ab7

Slot = 1
Version = 1.0.0
bootable : true
Flags : confirmed
6479df5a957bf7fa2e9b45e075788fd91e3fb15895affee06ff3b9cb4e546645
```
After this, the uploaded image becomes active and will be running in primary slot.
### Confirming the image
```
img_set_state confirm 0x1234 (Slave node address)
```
Should get some logs like this

```
Slot = 0
Version = 1.0.0
bootable : true
Flags : active confirmed
76609ae104fd47f74413d1c39501569605a12c2d17f17922cc3e417448244ab7

Slot = 1
Version = 1.0.0
bootable : true
Flags :
6479df5a957bf7fa2e9b45e075788fd91e3fb15895affee06ff3b9cb4e546645
```

