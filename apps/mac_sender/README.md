# UWB Mac Sender

## Overview
This example along with mac_recver example demonstrate how to use DW1000 mac interface.

### Building target for dwm1001

```no-highlight
newt target create mac_sender
newt target set mac_sender app=apps/mac_sender
newt target set mac_sender bsp=@decawave-uwb-core/hw/bsp/dwm1001
newt build mac_sender
newt create-image mac_sender 0.0.0.0
newt load mac_sender
```
