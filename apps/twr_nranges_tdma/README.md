# Ranging with n nodes using n+2 messages along with TDMA slotting.

## Overview
This example demonstrates the nrng use case--n ranges measurements with 2*n+2 frames. Here we use the CCP/TDMA to create a synchronous network with a superframe period of 1s and 160 TDMA slots. A tag[s] performs a range request[s] at the beginning of each TDMA slot. The nodes listen for a range request at the beginning [first 200us] of each TDMA slot. A tag can range to 1 to 16 nodes within a single TDMA slot. Each node responses sequential to the request based on their allocated node-slot--Note there are two slot mechanisms at work here; TDMA-slots and a node-slots. The node-slots are spaced approximately 270us apart within the TDMA slot with 16 nodes occupying about 4500us of the available 6300us windows. Practically a tag will range to a subset of available nodes or practically only a subset of the available nodes will be within range of the tag.

The example also illustrates the WCS (Wireless Clock Synchronization) capability, here the TOA (Time Of Arrival) timestamps are reported in master clock (node0) reference frame.

### Building target for (up to) 8 nodes

Master node (only one allowed per network):
```no-highlight
newt target create nrng_master_node
newt target set nrng_master_node app=apps/twr_nranges_tdma
newt target set nrng_master_node bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set nrng_master_node build_profile=debug
newt target amend nrng_master_node syscfg=PANMASTER_ISSUER=1
newt run nrng_master_node 0.1.0
```

Slave nodes
```no-highlight
newt target create nrng_slave_node
newt target set nrng_slave_node app=apps/twr_nranges_tdma
newt target set nrng_slave_node bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set nrng_slave_node build_profile=debug
newt target amend nrng_slave_node syscfg=NRANGES_ANCHOR=1
newt run nrng_slave_node 0.1.0
```

### Building target for tags
```
newt target create nrng_tag
newt target set nrng_tag app=apps/twr_nranges_tdma
newt target set nrng_tag bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set nrng_tag build_profile=debug
newt target amend nrng_tag syscfg=NRNG_NTAGS=4:NRNG_NNODES=8:NRNG_NFRAMES=16:NODE_START_SLOT_ID=0:NODE_END_SLOT_ID=7
newt run nrng_tag 0.1.0
```

If you'd prefer to be able to read the ranges in mm rather than getting the float cast to uint32_t as the output replace the tag target amend line above with the one below:

```
newt target amend nrng_tag syscfg=FLOAT_USER=1
```

**NOTE:** The value of NRNG_FRAMES must be atleast NRNG_NODES*2.


### NRNG profile FOM:

| profile       | Description  | Benchmark  |
| ------------- |:-------------:| -----:|
| nrng_ss | n TWR_SS ranges with n+2 messages. | 1860us for n=4, 2133us for n=6|

FOM = frame duration + TX_HOLDOFF + n * (frame duration + TX_GUARD_DELAY)

The number of nodes to range with can be configured by setting the **NRNG_NNODES** on tag app during build time,
   (ex: for 3 nodes, use this command while building tag app **newt target amend tag syscfg=NRNG_NNODES=3** )

To monitor the logs from the multiple tags in same PC, issue the following command to modify the nrng_tag target:

```
newt target amend nrng_tag syscfg=CONSOLE_RTT=0:CONSOLE_UART=1
```

Rebuild the app and run again.
Use Any serial Console app with 1000000 baudrate on PC to monitor the Logs.

### Expected Tag Output
```
{"utime": 88255157,"wcs": [23983557962832,893813779536,5637890867503,893813779536],"skew": 13742592839501479936}
{"utime": 88307220,"nrng": {"seq": 224,"mask": 127,"rng": ["8.088","8.039","1.320","8.426","7.131","6.604","7.026"],"uid": ["1946","5199","5235","2293","1436","5936","3958"]}}
{"utime": 88334106,"nrng": {"seq": 225,"mask": 127,"rng": ["8.065","8.067","1.329","8.421","7.122","6.636","7.026"],"uid": ["1946","5199","5235","2293","1436","5936","3958"]}}
{"utime": 88361022,"nrng": {"seq": 226,"mask": 127,"rng": ["8.076","8.093","1.322","8.541","7.108","6.585","6.444"],"uid": ["1946","5199","5235","2293","1436","5936","3958"]}}
{"utime": 88387908,"nrng": {"seq": 227,"mask": 127,"rng": ["8.088","8.074","1.231","8.635","7.110","6.592","6.439"],"uid": ["1946","5199","5235","2293","1436","5936","3958"]}}
{"utime": 88414794,"nrng": {"seq": 228,"mask": 127,"rng": ["8.041","8.065","1.317","8.468","7.101","6.599","6.453"],"uid": ["1946","5199","5235","2293","1436","5936","3958"]}}
{"utime": 88441680,"nrng": {"seq": 229,"mask": 127,"rng": ["8.079","8.004","1.294","8.738","7.138","6.592","6.451"],"uid": ["1946","5199","5235","2293","1436","5936","3958"]}}
{"utime": 88468566,"nrng": {"seq": 230,"mask": 127,"rng": ["8.090","8.039","1.249","8.529","7.103","6.634","7.044"],"uid": ["1946","5199","5235","2293","1436","5936","3958"]}}
{"utime": 88495452,"nrng": {"seq": 231,"mask": 127,"rng": ["8.046","8.088","1.301","8.623","7.115","6.578","6.472"],"uid": ["1946","5199","5235","2293","1436","5936","3958"]}}
{"utime": 88522338,"nrng": {"seq": 232,"mask": 127,"rng": ["8.060","8.114","1.264","8.419","7.115","6.629","5.968"],"uid": ["1946","5199","5235","2293","1436","5936","3958"]}}
{"utime": 88549224,"nrng": {"seq": 233,"mask": 127,"rng": ["8.088","8.100","1.299","8.731","7.112","6.599","6.517"],"uid": ["1946","5199","5235","2293","1436","5936","3958"]}}
{"utime": 88576110,"nrng": {"seq": 234,"mask": 127,"rng": ["8.036","8.100","1.303","8.742","7.112","6.618","7.073"],"uid": ["1946","5199","5235","2293","1436","5936","3958"]}}
{"utime": 88603027,"nrng": {"seq": 235,"mask": 127,"rng": ["8.086","8.046","1.200","8.618","7.122","6.620","7.056"],"uid": ["1946","5199","5235","2293","1436","5936","3958"]}}
{"utime": 88629882,"nrng": {"seq": 236,"mask": 127,"rng": ["8.053","8.112","1.320","8.613","7.143","6.594","6.529"],"uid": ["1946","5199","5235","2293","1436","5936","3958"]}}
{"utime": 88656799,"nrng": {"seq": 237,"mask": 127,"rng": ["8.060","8.097","1.320","8.412","7.098","6.650","7.026"],"uid": ["1946","5199","5235","2293","1436","5936","3958"]}}
{"utime": 88683654,"nrng": {"seq": 238,"mask": 127,"rng": ["8.051","8.083","1.278","8.423","7.117","6.648","6.456"],"uid": ["1946","5199","5235","2293","1436","5936","3958"]}}
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
node. This will show the active firmware on all nodes in the network. Example below:

```
441467 compat> panm list
441494 #idx, addr, role, slot, p,  lease, euid,             flags,          date-added, fw-ver
441494    0,  bad,    1,    0,  ,       , 010B1044C2230BAD,  1000, 1970-01-01T00:00:00, 0.0.4
441494    1,  ca8,    1,    1,  , 3597.8, 010B1044C2230CA8,  1000, 1970-01-01T00:00:05, 0.0.4
441495    2, 9211,    2,    0,  , 3597.8, 05A2610048E19211,  2000, 1970-01-01T00:00:58, 0.0.4
...
```

where role 1=anchor and 2=tag.


## 6. For PDOA on DWM1002 module. 

```no-highlight

newt target create dwm1002_nrng_slave_node
newt target set dwm1002_nrng_slave_node app=apps/twr_nranges_tdma
newt target set dwm1002_nrng_slave_node bsp=@mynewt-dw1000-core/hw/bsp/dwm1002
newt target set dwm1002_nrng_slave_node build_profile=debug
newt target amend dwm1002_nrng_slave_node syscfg=DW1000_DEVICE_0=1:DW1000_DEVICE_1=1:USE_DBLBUFFER=0:CIR_ENABLED=1
newt target amend dwm1002_nrng_slave_node syscfg=NRANGES_ANCHOR=1
#newt target amend dwm1002_nrng_slave_node syscfg=CONSOLE_RTT=0:CONSOLE_UART=1
newt run dwm1002_nrng_slave_node 0


```
