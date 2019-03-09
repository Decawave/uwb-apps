# Ranging with n nodes using 2n+2 messages along with TDMA slotting.

## Overview
This Demo uses 1 tag and 4 nodes for demonstrating tdma based slotting and nranges based ranging.

### Building target for 4 nodes
```no-highlight
newt target create node
newt target set node app=apps/twr_node_nranges_tdma
newt target set node bsp=@mynewt-dw1000-core/hw/bsp/dwm1001

newt target amend node syscfg=DEVICE_ID=0x1000:SLOT_ID=0:NRNG_NNODES=16:NRNG_NFRAMES=32:NODE_START_SLOT_ID=0:NODE_END_SLOT_ID=7
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x1001:SLOT_ID=1
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x1002:SLOT_ID=2
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x1003:SLOT_ID=3
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x1004:SLOT_ID=4
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x1005:SLOT_ID=5
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x1006:SLOT_ID=6
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x1007:SLOT_ID=7
newt run node 0
```
### Building target for tags
```
newt target create tag
newt target set tag app=apps/twr_tag_nranges_tdma
newt target set tag bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set tag build_profile=debug
newt target amend tag syscfg=NRNG_NNODES=16:NRNG_NFRAMES=32:NODE_START_SLOT_ID=0:NODE_END_SLOT_ID=7
newt run tag 0
```
**NOTE:** The value of NRNG_FRAMES must be atleast NRNG_NODES*2.

**NOTE:** The number of slots is inversly proportional to the number of nodes in the network. The current configuration 
          of TDMA_SLOTS in syscfg.yml is set to 128 for 4 nodes. Now if using 8 nodes this should be reduced to ~64 slots

Current FOM(Time taken for 1 range) for 4 nodes is ~6.5 to 7ms and CCP period is 1s. The formula to calculate number of slots will be Slots = CCP_PERIOD / FOM( N Nodes )

The number of nodes to range with can be configured by setting the **NRNG_NNODES** on tag app during build time,
   (ex: for 3 nodes, use this command while building tag app **newt target amend tag syscfg=NRNG_NNODES=3** )

**NOTE:** To monitor the logs from the multiple tags in same PC, Do the following changes in the apps/twr_tag_tdma_nranges/syscfg.yml
```
    CONSOLE_RTT: 0
    CONSOLE_UART: 1

```
  Rebuild the app and run again.
  Use Any serial Console app with 1000000 baudrate on PC to monitor the Logs.

