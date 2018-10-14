# Ranging with n nodes using 2n+2 messages

## Overview

The Double sided asymmetric Two way ranging between **one tag** and **n nodes** can be achieved in **2n+2** messages.

In this example
1. Tag application (apps/twr_tag_nranges) initiates ranging in **DWT_DS_TWR_NRNG** mode, and waits for the **n** responses.
2. The node application (apps/twr_node_nranges) responds to the **DWT_DS_TWR_NRNG** message with mode **DWT_DS_TWR_NRNG_T1**, after certain delay based on its **slot_id**.
3. Tag on successfull reception of **n** responses (or atleast one response) will start second round of sending request messages with **DWT_DS_TWR_NRNG_T2** mode, and waits for the **n** final responses.
4. Upon receiving the **n** final responses (or atleast one final response) calculates the TOF and range for each node.
5. The number of nodes to range with **'n'** is a configurable parameter of tag application, during build time.

### 1. Building applications for 4 nodes and one tag
```no-highlight
newt target create node
newt target set node app=apps/twr_node_nranges
newt target set node bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target amend node syscfg=DEVICE_ID=0x1001:SLOT_ID=1
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x1002:SLOT_ID=2
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x1003:SLOT_ID=3
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x1004:SLOT_ID=4
newt run node 0


newt target create tag
newt target set tag app=apps/twr_tag_nranges
newt target set tag bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set tag build_profile=debug
newt target amend tag syscfg=N_NODES=4
newt run tag 0

```

6. The number of nodes to range with can be configured by setting the **N_NODES** on tag app during build time,
   (ex: for 3 nodes, use this command while building tag app **newt target amend tag syscfg=N_NODES=3** )
