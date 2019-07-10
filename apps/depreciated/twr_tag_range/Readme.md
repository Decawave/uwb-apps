# Range Service

## Overview

Range service allows a device to initiate ranging with multiple target devices and get the range values corresponding to each target device.
The service accepts number of target devices and their addresses during initialization. It initiates ranging with each target device in a sequence and returns the time stamps used to calculate the ToF and range. The frequency of ranging is controlled by a MACRO setting.

In this example, application (apps/twr_node_range) initiates ranging with multiple targets (apps/twr_tag_range) and prints the range values with each target and repeats the same.

### 1. Build range example applications
```no-highlight
newt target create node
newt target set node app=apps/twr_node_range
newt target set node bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set node build_profile=debug 
newt target amend node syscfg=DW_DEVICE_ID_0=0xDEC0
newt run node 0

newt target create tag1
newt target set tag1 app=apps/twr_tag_range
newt target set tag1 bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set tag1 build_profile=debug
newt target amend tag1 syscfg=DW_DEVICE_ID_0=0xDEC1
newt run tag1 0

newt target create tag2
newt target set tag2 app=apps/twr_tag_range
newt target set tag2 bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set tag2 build_profile=debug
newt target amend tag2 syscfg=DW_DEVICE_ID_0=0xDEC2
newt run tag2 0
```

**NOTE**: If pan is enabled on the node and tags, then first power ON tags, pan_master followed by node. Use the following steps to create and build pan_master.

newt target create pan
newt target set pan app=apps/pan_master
newt target set pan bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set pan build_profile=debug
newt run pan 0

