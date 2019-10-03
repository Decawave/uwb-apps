# Ranging with n nodes using 2n+2 messages along with TDMA slotting.

## Overview



### Building targets

Master node (only one allowed per network):

```no-highlight
newt target create rtdoa_master
newt target set rtdoa_master app=apps/rtdoa_node
newt target set rtdoa_master bsp=@decawave-uwb-core/hw/bsp/dwm1001
newt target amend rtdoa_master syscfg="MASTER_NODE=1"
newt run rtdoa_master 0.1.0
```

Slave node, up to 16 per network (for now):

```no-highlight
newt target create rtdoa_slave
newt target set rtdoa_slave app=apps/rtdoa_node
newt target set rtdoa_slave bsp=@decawave-uwb-core/hw/bsp/dwm1001
newt target amend rtdoa_slave syscfg="MASTER_NODE=0"
newt run rtdoa_slave 0.1.0
```

Tags:

```no-highlight
newt target create rtdoa_tag
newt target set rtdoa_tag app=apps/rtdoa_tag
#newt target set rtdoa_tag bsp=@decawave-uwb-core/hw/bsp/dwm1001
newt target set rtdoa_tag bsp=@decawave-uwb-core/hw/bsp/dwm1003
#newt target amend rtdoa_tag syscfg="MASTER_NODE=0"
newt run rtdoa_tag 0.1.0
```


