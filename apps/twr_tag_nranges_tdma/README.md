# Ranging with n nodes using 2n+2 messages along with TDMA slotting.

## Overview
This example pair demonstrates the nrng use case--n ranges measurements with 2*n+2 frames. Here we use the CCP/TDMA to create a synchronous network with a superframe period of 1s and 160 TDMA slots. A tag[s] performs a range request[s] at the beginning of each TDMA slot. The nodes listen for a range request at the beginning [first 200us] of each TDMA slot. A tag can range to 1 to 16 nodes within a single TDMA slot. Each node responses sequential to the request based on their allocated node-slot--Note there are two slot mechanisms at work here; TDMA-slots and a node-slots. The node-slots are spaced approximately 270us apart within the TDMA slot with 16 nodes occupying about 4500us of the available 6300us windows. Practically a tag will range to a subset of available nodes or practically only a subset of the available nodes will be within range of the tag. 

The example also illustrates the WCS (Wireless Clock Synchronization) capability, here the TOA (Time Of Arrival) timestamps are reported in master clock (node0) reference frame.

### Building target for 4 nodes
```no-highlight
newt target create node
newt target set node app=apps/twr_node_nranges_tdma
newt target set node bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target amend node syscfg=DEVICE_ID=0x1001:SLOT_ID=0
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x1002:SLOT_ID=1
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x1003:SLOT_ID=2
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x1004:SLOT_ID=3
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x1005:SLOT_ID=4
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x1006:SLOT_ID=5
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x1007:SLOT_ID=6
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x1008:SLOT_ID=7
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x1009:SLOT_ID=8
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x100A:SLOT_ID=9
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x100B:SLOT_ID=10
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x100C:SLOT_ID=11
newt run node 0
newt target amend node syscfg=DEVICE_ID=0x100D:SLOT_ID=12
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


### NRNG profile FOM:

| profile       | Description  | Benchmark  |
| ------------- |:-------------:| -----:|
| nrng_ss | n TWR_SS ranges with 2*n+2 messages. FOM = TX_HOLDOFF + n * (frame duration + TX_GUARD_DELAY) | 1860us for n=4, 2133us for n=6|


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
{"utime": 74796952,"wcs": [481514960962,380619028866,2],"skew": 4519785054260428800,"nT": [1,55,54]}
{"utime": 74807858,"nrng": {"seq_num": 41,"mask": 255,"rng": [1093002180,1092721840,1092468552,1243450022,1092707086,1092793155,1092950538,1092439042],"toa": [908142286,908142264,908142167,908142175,908142149,908142148,908142176,908142044]}}
{"utime": 74861631,"nrng": {"seq_num": 42,"mask": 255,"rng": [1092886601,1092724300,1092537407,1250705272,1092687413,1092719381,1092881683,1092446420],"toa": [49159449,49159506,49159361,49159369,49159327,49159329,49159334,49159236]}}
{"utime": 74915404,"nrng": {"seq_num": 43,"mask": 255,"rng": [1092921029,1092729218,1092520193,1092601344,1092603803,1092748891,1092921029,1092446420],"toa": [3485143955,3485144044,3485143850,3485143851,3485143760,3485143833,3485143836,3485143707]}}
{"utime": 74969173,"nrng": {"seq_num": 44,"mask": 255,"rng": [1092933324,1092719381,1092507897,1092662822,1092712004,1092716922,1092884142,1092466092],"toa": [2626160663,2626160774,2626160522,2626160556,2626160435,2626160509,2626160481,2626160365]}}
{"utime": 75022947,"nrng": {"seq_num": 45,"mask": 255,"rng": [1092955456,1092771023,1092596426,1092648067,1092621017,1092802991,1092913651,1092515275],"toa": [1767177846,1767178074,1767177740,1767177749,1767177588,1767177726,1767177673,1767177535]}}
{"utime": 75076731,"nrng": {"seq_num": 46,"mask": 255,"rng": [1092957915,1092743972,1092584130,1092645608,1092638231,1092699708,1092886601,1092517734],"toa": [908194527,908194837,908194415,908194395,908194290,908194352,908194357,908194239]}}
{"utime": 75130504,"nrng": {"seq_num": 47,"mask": 255,"rng": [1092921029,1092714463,1092544784,1092618558,1092662822,1092746432,1092962834,1092500520],"toa": [49211696,49212099,49211607,49211581,49211501,49211572,49211576,49211415]}}
{"utime": 75184270,"nrng": {"seq_num": 48,"mask": 255,"rng": [1092901356,1092785777,1092483306,1092633312,1092743972,1092707086,1092918570,1092443960],"toa": [3485196170,3485196680,3485196066,3485196075,3485196031,3485196051,3485196066,3485195904]}}
{"utime": 75238043,"nrng": {"seq_num": 49,"mask": 255,"rng": [1092972670,1092736595,1092539866,1092650526,1092608721,1092687413,1092925947,1092372646],"toa": [2626212851,2626213438,2626212769,2626212747,2626212689,2626212689,2626212747,2626212582]}}
{"utime": 75291817,"nrng": {"seq_num": 50,"mask": 255,"rng": [1092933324,1092741513,1092517734,1092633312,1092606262,1092729218,1092906274,1092534948],"toa": [1767230054,1767230674,1767229955,1767229946,1767229890,1767229905,1767229952,1767229835]}}
{"utime": 75345590,"nrng": {"seq_num": 51,"mask": 255,"rng": [1092893978,1092748891,1092495602,1092598885,1092682495,1092734136,1092906274,1092498061],"toa": [908246740,908247426,908246636,908246627,908246561,908246617,908246615,908246499]}}
{"utime": 75399363,"nrng": {"seq_num": 52,"mask": 255,"rng": [1092911192,1092707086,1092500520,1092591507,1092677576,1092707086,1092923488,1092461174],"toa": [49263932,49264671,49263812,49263828,49263740,49263792,49263803,49263669]}}
{"utime": 75453136,"nrng": {"seq_num": 53,"mask": 255,"rng": [1092849714,1092719381,1092532489,1092603803,1092586589,1092707086,1092889060,1092468552],"toa": [3485248438,3485249216,3485248321,3485248321,3485248199,3485248290,3485248270,3485248147]}}
{"utime": 75506917,"nrng": {"seq_num": 54,"mask": 255,"rng": [1092869387,1092724300,1092483306,1092598885,1092633312,1092736595,1092967752,1092436583],"toa": [2626265127,2626266008,2626264985,2626265010,2626264854,2626264978,2626264972,2626264793]}}
{"utime": 75560690,"nrng": {"seq_num": 55,"mask": 255,"rng": [1092896438,1092731677,1092574294,1092611180,1092621017,1092670199,1092984966,1092387401],"toa": [1767282326,1767283285,1767282189,1767282194,1767282057,1767282150,1767282171,1767281988]}}
{"utime": 75614463,"nrng": {"seq_num": 56,"mask": 255,"rng": [1092827582,1092748891,1092468552,1092667740,1092581671,1092761186,1092925947,1092483306],"toa": [908298974,908300055,908298859,908298886,908298750,908298818,908298853,908298707]}}
{"utime": 75668229,"nrng": {"seq_num": 57,"mask": 255,"rng": [1092923488,1092716922,1092485765,1092616098,1092652985,1092692331,1092913651,1092478388],"toa": [49316170,49317321,49316047,49316067,49315946,49316000,49316041,49315910]}}
{"utime": 75722002,"nrng": {"seq_num": 58,"mask": 255,"rng": [1093024312,1092709545,1092539866,1092606262,1092640690,1092724300,1092952997,1092463633],"toa": [3485300678,3485301890,3485300543,3485300558,3485300451,3485300522,3485300548,3485300415]}}
{"utime": 75775776,"nrng": {"seq_num": 59,"mask": 255,"rng": [1092967752,1092771023,1092468552,1092628394,1092539866,1092746432,1092950538,1092461174],"toa": [2626317356,2626318646,2626317230,2626317239,2626317131,2626317192,2626317237,2626317096]}}
{"utime": 75829549,"nrng": {"seq_num": 60,"mask": 255,"rng": [1092940702,1092687413,1092608721,1092616098,1092638231,1092820205,1092928406,1092468552],"toa": [1767334552,1767335888,1767334459,1767334447,1767334322,1767334424,1767334428,1767334273]}}
{"utime": 75872424,"wcs": [550234437698,449338401071,2],"skew": 4519790881815396352,"nT": [1,56,55]}


```