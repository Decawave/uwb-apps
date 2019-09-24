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

# Decawave blemesh Example

## Overview
The tag_mesh is a simple example that showcase the TDMA features along with mesh facility. The default behavior divides the TDMA_PERIOD into TDMA_NSLOTS and allocates slots to a single ranging task and able to provision the devices into mesh.

To run the below applications which are configured with UART, change the pins in hw/bsp/dwm1001/syscfg.yml

1. To erase the default flash image that shipped with the DWM1001 boards.

```no-highlight
$ JLinkExe -device nRF52 -speed 4000 -if SWD
J-Link>erase
J-Link>exit
$ 
```

2. On 1st dwm1001-dev board build the mesh node (twr_node_tdma) application for the DWM1001 module. 

```no-highlight

newt target create node_tdma
newt target set node_tdma app=apps/twr_node_tdma
newt target set node_tdma bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set node_tdma build_profile=optimized
newt target set node_tdma syscfg=BLE_MESH_PB_GATT=1:BLE_MESH_DEV_UUID='(uint8_t[16]){0x22, 0x20, 0}'
newt build node_tdma
newt create-image node_tdma 1.0.0
newt load node_tdma

```

3. On 2nd dwm1001-dev board build the mesh tag (tag_mesh) applications for the DWM1001 module. 

```no-highlight

newt target create tag_mesh
newt target set tag_mesh app=apps/tag_mesh
newt target set tag_mesh bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set tag_mesh build_profile=optimized
newt target set tag_mesh syscfg=BLE_MESH_PB_GATT=1:BLE_MESH_DEV_UUID='(uint8_t[16]){0x22, 0x21, 0}'
newt build tag_mesh
newt create-image tag_mesh 1.0.0
newt load tag_mesh

```

4. After flashing both the devices, your console will have both mesh advertising and ranging happening at a time.You have to provision the devices using mobile mesh app (Bluetooth mesh by siliocn labs).

Below are the steps needed to follow for provisioning.To provision the devices,a bluetooth mesh app is required.
Before this, you need to open the logs of 2 devices in their appropriate terminals, to check for OOB, an authentication key to do provisioning.

1. Install bluetooth mesh app in your mobile.
2. After flashing the devices, open mesh app in mobile.
3. Enable bluetooth which the app asks for.
4. Then a page is opened showing network and provision.
5. Click on provision and scan for the devices.
6. You can see nimble-mesh named devices in mobile with provision icon.
7. Click on provision icon of one device and check the logs of both devices to know which is being provisioned.On the console, you can see
      ```
       proxy_connected: conn_handle 1
      ```
8. Then it asks to enter device name, enter as 1, immediately you get a OOB on conosle, an authentication key to add the device to mesh.
9. Enter the key and a configuration window is opened, which shows device_name, proxy, relay, functionality and group.
10. Click on functionality and select on/off.
11. Click on group and select Demo group and apply the changes.
12. Then a bulb like icon is shown on mobile.click on it and check for LED functionality of your device.

After this come back to the provision page and follow same steps to provision other devices.



