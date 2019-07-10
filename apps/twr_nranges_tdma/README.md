# Ranging with n nodes using 2n+2 messages along with TDMA slotting.

## Overview
This example demonstrates the nrng use case--n ranges measurements with 2*n+2 frames. Here we use the CCP/TDMA to create a synchronous network with a superframe period of 1s and 160 TDMA slots. A tag[s] performs a range request[s] at the beginning of each TDMA slot. The nodes listen for a range request at the beginning [first 200us] of each TDMA slot. A tag can range to 1 to 16 nodes within a single TDMA slot. Each node responses sequential to the request based on their allocated node-slot--Note there are two slot mechanisms at work here; TDMA-slots and a node-slots. The node-slots are spaced approximately 270us apart within the TDMA slot with 16 nodes occupying about 4500us of the available 6300us windows. Practically a tag will range to a subset of available nodes or practically only a subset of the available nodes will be within range of the tag.

The example also illustrates the WCS (Wireless Clock Synchronization) capability, here the TOA (Time Of Arrival) timestamps are reported in master clock (node0) reference frame.

### Building target for 4 nodes

Master node (only one allowed per network):
```no-highlight
newt target create master_node
newt target set master_node app=apps/twr_nranges_tdma
newt target set master_node bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target amend master_node syscfg=PANMASTER_ISSUER=1
newt run master_node 0
```

Slave nodes
```no-highlight
newt target create slave_node
newt target set slave_node app=apps/twr_nranges_tdma
newt target set slave_node bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target amend slave_node syscfg=NRANGES_ANCHOR=1
newt run slave_node 0
```

### Building target for tags
```
newt target create tag
newt target set tag app=apps/twr_nranges_tdma
newt target set tag bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set tag build_profile=debug
newt target amend tag syscfg=NRNG_NNODES=4:NRNG_NFRAMES=8:NODE_START_SLOT_ID=0:NODE_END_SLOT_ID=7
newt run tag 0
```

If you'd prefer to be able to read the ranges in mm rather than getting the float cast to uint32_t as the output replace the tag target amend line above with the one below:

```
newt target amend tag syscfg=NRNG_NNODES=4:NRNG_NFRAMES=8:NODE_START_SLOT_ID=0:NODE_END_SLOT_ID=7:NRNG_HUMAN_READABLE_RANGES=1
```

**NOTE:** The value of NRNG_FRAMES must be atleast NRNG_NODES*2.


### NRNG profile FOM:

| profile       | Description  | Benchmark  |
| ------------- |:-------------:| -----:|
| nrng_ss | n TWR_SS ranges with 2*n+2 messages. | 1860us for n=4, 2133us for n=6|

FOM = frame duration + TX_HOLDOFF + n * (frame duration + TX_GUARD_DELAY)

The number of nodes to range with can be configured by setting the **NRNG_NNODES** on tag app during build time,
   (ex: for 3 nodes, use this command while building tag app **newt target amend tag syscfg=NRNG_NNODES=3** )

**NOTE:** To monitor the logs from the multiple tags in same PC, Do the following changes in the apps/twr_tag_tdma_nranges/syscfg.yml
```
    CONSOLE_RTT: 0
    CONSOLE_UART: 1

```
Rebuild the app and run again.
Use Any serial Console app with 1000000 baudrate on PC to monitor the Logs.

### Expected Tag Output
```
{"utime": 1247523865,"wcs": [756377993282,548259333440,3],"skew": 4524316331544936448,"nT": [1,203,202]}
{"utime": 1247535632,"nrng": {"seq_num": 33,"mask": 255,"rng": [1090183467,1090173631,1089239167,1089470324,1074147011,1074078156,1083927478,1083686485],"tdoa": [3348,3404,2776,2779,362,372,1521,1515]}}
{"utime": 1247562512,"nrng": {"seq_num": 34,"mask": 255,"rng": [1090203140,1090139203,1089199821,1089465406,1074156848,1074048647,1083784850,1083838950],"tdoa": [3347,3390,2770,2775,337,366,1517,1523]}}
{"utime": 1247589398,"nrng": {"seq_num": 35,"mask": 255,"rng": [1090198222,1090139203,1089249003,1089489997,1074314231,1074156848,1083893051,1083838950],"tdoa": [3348,3378,2773,2776,330,372,1532,1508]}}
...
```


### Node specific commands

The node firmware can act both as slave_node and master_node by changing the config parameters
in runtime. To view the configuration parameters avaiable use the ```config dump``` command
in the console connected to the RTT (```nc localhost 19021```) or UART of the board.

To promote a slave_node to master_node use the following commands:

```
config uwb/role 0x7
config save
reset
```

To demote a master_node to slave_node use the following commands:

```
config uwb/role 0x4
config save
reset
```

**NOTE** Changing the uwb/role to 0x0 makes that board act as a tag. 

### Broadcast of firmware from one node to all others

When working with a network of nodes it can be useful to update them all at once.
In this example we will distribute version 0.1.2 to a network of nodes having the version
0.1.1. First, program one node with the new firmware, it doesn't have to be the master:

```
newt run slave_node 0.1.2
```

Before proceeding, make sure no tags are active as the tag-slot is shared with the
uwb_nmgr layer carrying the firmware update.
Then, connected to the console of the recently programmed node run:

```
bota txim 0xffff 0
```

where ```bota``` is the command to Broadcast Over The Air updates, ```txim``` is transmit
image, 0xffff is the broadcast address (all nodes), and 0 is the image we'd like to send.
Instead of tranmitting image 0 (the current running) you can also transmit image 1.

This command will use the tag slots to send data over uwb to all other nodes. When the
transfere is complete the receiving node will check that the checksum is correct and if so
boot into the new image. If not, it will not do anything. If there are any errors during the
transfere this will result in a failed checksum, but the command can be run and the image
transmitted several times. The receiving node fills in it's image with the parts it
successfully receives until it succeeds or no further transmissions are done.

In short, if the command doesn't succeed in updating all nodes at first, try again. 
Note that the command runs for about a minute and you should let it finish before starting
again.

To check if all nodes are on the same firmware use the ```panm list``` command on the master
node. This will show the active firmware on all nodes in the network.
