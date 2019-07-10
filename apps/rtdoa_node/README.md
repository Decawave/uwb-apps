# Ranging with n nodes using 2n+2 messages along with TDMA slotting.

## Overview



### Building targets

Master node (only one allowed per network):

```no-highlight
newt target create master_node
newt target set master_node app=apps/rtdoa_node
newt target set master_node bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target amend master_node syscfg="MASTER_NODE=1"
newt run master_node 0.1.0
```

Slave node, up to 16 per network (for now):

```no-highlight
newt target create slave_node
newt target set slave_node app=apps/rtdoa_node
newt target set slave_node bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target amend slave_node syscfg="MASTER_NODE=0"
newt run slave_node 0.1.0
```

Tags:

```no-highlight
newt target create rtdoa_tag
newt target set rtdoa_tag app=apps/rtdoa_tag
newt target set rtdoa_tag bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
#newt target amend rtdoa_tag syscfg="MASTER_NODE=0"
newt run rtdoa_tag 0.1.0
```


