# UWB Mac Sender

## Overview
This example along with mac_sender example demonstrate how to use DW1000 mac interface.

## Building target for dwm1001

```no-highlight
newt target create mac_recver
newt target set mac_recver app=apps/mac_recver
newt target set mac_recver bsp=@decawave-uwb-core/hw/bsp/dwm1001
newt build mac_recver
newt create-image mac_recver 0.0.0.0
newt load mac_recver
```

## Standalone usage
* Open serial port (baud:115200)
* Type `receive` + enter to enter receive mode
* Type `sleep` + enter to enter receive mode

## Wireshark support
* Copy nrf802154_sniffer.py from script directory to extcap directory for Wireshark (usually: `/usr/lib/x86_64-linux-gnu/wireshark/extcap`).
* Change nrf802154_sniffer.py to runnable using the following command: `sudo chmod +x nrf802154_sniffer.py`
* Open Wireshark, there will be a interface named: `nRF Sniffer for 802.15.4: /dev/ttyACM*`
* Click to the interface and choose channel. Currently, only channel field is supported. The default channel in mac_sender example is 5. So, channel 5 should be chose for bringing up.
* If there is device running mac_sender example, the packets should be shown in a few seconds.