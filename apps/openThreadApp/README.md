
<!--
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#  KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
-->

# Decawave DW1000 Application openThreadApp

## Overview
**Openthread + Mynewt + NRF52832 + DW1000**

   Openthread is a mesh networking protocol based on the Thread specifications.
More Information on the Openthread can be found here https://github.com/openthread/openthread.

   In this project Openthread is ported on the mynewt supporting DWM1001 platfrom. This project uses the Openthread stack in the form of libraries which are built standalone and copied to the **mynewt-dw1000-core/lib/openthread/src/ot_prebuilt**. The various abstraction layers are written in the mynewt style to run OT on the DWM1001, the abstraction layers can be found in the **mynewt-dw1000-core/lib/openthread/src** and relative headers in **mynewt-dw1000-core/lib/openthread/include/openthread**.
This example application demonstrates a minimal OpenThread application that exposes the OpenThread configuration and management interfaces via a basic command-line interface.

The steps below take you through the minimal steps required to ping one emulated Thread device from another emulated Thread device.

**NOTE** : Currently only nrf52xxx platform & dwm1001 bsp is supported for this application.

## Generating the Openthread static libraries
1. This step is optional, as already the static libraries are built and copied to openthread/lib.
2. This step required , only if any changes/additions required to do in the ot stack, or to debug ot stack.
3. Do the following steps in Linux PC through the Command Line.
```no-highlight
git clone https://github.com/openthread/openthread.git
cd openthread
git checkout thread-reference-20180926
./bootstrap
make -f examples/Makefile.nrf52840
(OR )
make -f ./examples/Makefile-nrf52840 FULL_LOGS=1    #( Build With Debug Logs enabled)

```
The static libraries of openthread stack can found in **output/nrf52840/lib/**


## Building openthread mynewt app

1. Download and install Apache Newt.
   You will need to download the Apache Newt tool, as documented in the [Getting Started Guide](http://mynewt.apache.org/latest/get_started/index.html).
   
**Prerequisites:** You should follow the generic tutorials at http://mynewt.apache.org/latest/tutorials/tutorials.html, particularly the basic Blinky example that will guide you through the basic setup.

2. Download the DW1000 Mynewt apps.

```no-highlight
    git clone git@github.com:Decawave/mynewt-dw1000-apps.git
    cd mynewt-dw1000-apps
    git checkout openthread
```

3. Running the newt install command downloads the apache-mynewt-core, mynewt-dw1000-core, and mynewt-timescale-lib packages, these are dependent repos of the mynewt-dw1000-apps project and are automatically checked-out by the newt tools.

```no-highlight
    $ newt install -v
```

4. Copying the Openthread Libraries mynewt project.
   (This step is required only if any updates/changes done in the actual OT source code, as already standard ot libraries are available).
 Copy **libmbedcrypto.a  libopenthread-cli-ftd.a  libopenthread-diag.a  libopenthread-ftd.a   libopenthread-platform-utils.a** into **repos/mynewt-dw1000-core/lib/openthread/src/ot_prebuilt/**

## Erase the default flash image that shipped with the DWM1001.

```no-highlight
$ JLinkExe -device nRF52 -speed 4000 -if SWD
SEGGER J-Link Commander V5.12c (Compiled Apr 21 2016 16:05:51)
DLL version V5.12c, compiled Apr 21 2016 16:05:45

Connecting to J-Link via USB...O.K.
Firmware: J-Link OB-SAM3U128-V2-NordicSemi compiled Mar 15 2016 18:03:17
Hardware version: V1.00
S/N: 682863966
VTref = 3.300V


Type "connect" to establish a target connection, '?' for help
J-Link>erase
Cortex-M4 identified.
Erasing device (0;?i?)...
Comparing flash   [100%] Done.
Erasing flash     [100%] Done.
Verifying flash   [100%] Done.
J-Link: Flash download: Total time needed: 0.363s (Prepare: 0.093s, Compare: 0.000s, Erase: 0.262s, Program: 0.000s, Verify: 0.000s, Restore: 0.008s)
Erasing done.
J-Link>exit
$
```

### Build and load the bootloader applicaiton for the DWM1001 target.

(executed from the mynewt-dw1000-app directory).

```no-highlight

newt target create dwm1001_boot
newt target set dwm1001_boot app=@apache-mynewt-core/apps/boot
newt target set dwm1001_boot bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set dwm1001_boot build_profile=optimized
newt build dwm1001_boot
newt create-image dwm1001_boot 1.0.0
newt load dwm1001_boot

```
### Build and flash the openThreadApp app on two devices

```no-highlight
newt target create openThreadApp
newt target set openThreadApp app=apps/openThreadApp
newt target set openThreadApp bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
newt target set openThreadApp build_profile=optimized
newt build openThreadApp
newt create-image openThreadApp 1.0.0
newt load openThreadApp
```
## Basic Testing

#### 1. Start node 1

Set the PAN ID:
```bash
003339 compat> ot panid 0xdeca
004039 panid 0xdeca
004039 Done
```

Bring up the IPv6 interface:
```bash
004039 > compat> ot ifconfig up
004626 ifconfig up
004626 Done
```

Start Thread protocol operation:
```bash
004627 > compat> ot thread start
005081 thread start
005081 Done
```

Wait a few seconds and verify that the device has become a Thread Leader:
```bash
005081 > compat> ot state
005731 state
005731 leader
005731 Done
```

View IPv6 addresses assigned to Node 1's Thread interface:
```bash
004942 compat> ot ipaddr
006271 ipaddr
006271 fdde:ad00:beef:0:0:ff:fe00:fc00
006271 fdde:ad00:beef:0:0:ff:fe00:3800
006271 fdde:ad00:beef:0:660a:ad51:f498:3bdf
006271 fe80:0:0:0:cc73:16ba:9437:db7e
006271 Done
```
#### 2. Start node 2

Set the PAN ID:
```bash
000172 compat> ot panid 0xdeca
003957 panid 0xdeca
003957 Done
```

Bring up the IPv6 interface:
```bash
003957 > compat> ot ifconfig up
004455 ifconfig up
004455 Done
```

Start Thread protocol operation:
```bash
004455 > compat> ot thread start
004848 thread start
004848 Done
```

Wait a few seconds and verify that the device has become a Thread Router/Child:
```bash
005183 > compat> ot state
005522 state
005522 child
005522 Done
```

#### 3. Ping Node 1 from Node 2
```bash
015281 compat> ot ping fdde:ad00:beef:0:660a:ad51:f498:3bdf
015559 ping fdde:ad00:beef:0:660a:ad51:f498:3bdf
015559 > compat> 8 bytes from fdde:ad00:beef:0:660a:ad51:f498:3bdf: icmp_seq=3 hlim=64 time=9ms
```

## OpenThread CLI Reference

The OpenThread CLI exposes configuration and management APIs via a command line interface. Use the CLI to play with OpenThread, which can also be used with additional application code. The
OpenThread test scripts use the CLI to execute test cases.

### OpenThread Command List
**NOTE** : To test any CLI command. Please use the follwing format
```
           ot <command> <args>
```

* [autostart](#autostart)
* [blacklist](#blacklist)
* [bufferinfo](#bufferinfo)
* [channel](#channel)
* [child](#child-list)
* [childmax](#childmax)
* [childtimeout](#childtimeout)
* [contextreusedelay](#contextreusedelay)
* [counter](#counter)
* [dataset](#dataset-help)
* [delaytimermin](#delaytimermin)
* [discover](#discover-channel)
* [eidcache](#eidcache)
* [eui64](#eui64)
* [extaddr](#extaddr)
* [extpanid](#extpanid)
* [factoryreset](#factoryreset)
* [hashmacaddr](#hashmacaddr)
* [ifconfig](#ifconfig)
* [ipaddr](#ipaddr)
* [ipmaddr](#ipmaddr)
* [joinerport](#joinerport-port)
* [keysequence](#keysequence-counter)
* [leaderdata](#leaderdata)
* [leaderpartitionid](#leaderpartitionid)
* [leaderweight](#leaderweight)
* [linkquality](#linkquality-extaddr)
* [masterkey](#masterkey)
* [mode](#mode)
* [netdataregister](#netdataregister)
* [networkdiagnostic](#networkdiagnostic-get-addr-type-)
* [networkidtimeout](#networkidtimeout)
* [networkname](#networkname)
* [panid](#panid)
* [parent](#parent)
* [ping](#ping-ipaddr-size-count-interval)
* [pollperiod](#pollperiod-pollperiod)
* [prefix](#prefix-add-prefix-pvdcsr-prf)
* [promiscuous](#promiscuous)
* [releaserouterid](#releaserouterid-routerid)
* [reset](#reset)
* [rloc16](#rloc16)
* [route](#route-add-prefix-s-prf)
* [router](#router-list)
* [routerdowngradethreshold](#routerdowngradethreshold)
* [routerrole](#routerrole)
* [routerselectionjitter](#routerselectionjitter)
* [routerupgradethreshold](#routerupgradethreshold)
* [scan](#scan-channel)
* [singleton](#singleton)
* [state](#state)
* [thread](#thread-start)
* [txpowermax](#txpowermax)
* [version](#version)
* [whitelist](#whitelist)
* [diag](#diag)

## OpenThread Command Details

### autostart true

Automatically start Thread on initialization.

```bash
> autostart true
Done
```

### autostart false

Don't automatically start Thread on initialization.

```bash
> autostart false
Done
```

### autostart

Show the status of automatically starting Thread on initialization.

```bash
> autostart
false
Done
```

### blacklist

List the blacklist entries.

```bash
> blacklist
Enabled
166e0a0000000002
166e0a0000000003
Done
```

### blacklist add \<extaddr\>

Add an IEEE 802.15.4 Extended Address to the blacklist.

```bash
> blacklist add 166e0a0000000002
Done
```

### blacklist clear

Clear all entries from the blacklist.

```bash
> blacklist clear
Done
```

### blacklist disable

Disable MAC blacklist filtering.

```bash
> blacklist disable
Done
```

### blacklist enable

Enable MAC blacklist filtering.

```bash
> blacklist enable
Done
```

### blacklist remove \<extaddr\>

Remove an IEEE 802.15.4 Extended Address from the blacklist.

```bash
> blacklist remove 166e0a0000000002
Done
```

### bufferinfo

Show the current message buffer information.

```bash
> bufferinfo
total: 40
free: 40
6lo send: 0 0
6lo reas: 0 0
ip6: 0 0
mpl: 0 0
mle: 0 0
arp: 0 0
coap: 0 0
Done
```

### channel

Get the IEEE 802.15.4 Channel value.

```bash
> channel
11
Done
```

### channel \<channel\>

Set the IEEE 802.15.4 Channel value.

```bash
> channel 11
Done
```

### child list

List attached Child IDs.

```bash
> child list
1 2 3 6 7 8
Done
```

### child table

Print table of attached children.

```bash
> child table
| ID  | RLOC16 | Timeout    | Age        | LQI In | C_VN |R|S|D|N| Extended MAC     |
+-----+--------+------------+------------+--------+------+-+-+-+-+------------------+
|   1 | 0xe001 |        240 |         44 |      3 |  237 |1|1|1|1| d28d7f875888fccb |
|   2 | 0xe002 |        240 |         27 |      3 |  237 |0|1|0|1| e2b3540590b0fd87 |
Done
```

### child \<id\>

Print diagnostic information for an attached Thread Child.  The `id` may be a Child ID or an RLOC16.

```bash
> child 1
Child ID: 1
Rloc: 9c01
Ext Addr: e2b3540590b0fd87
Mode: rsn
Net Data: 184
Timeout: 100
Age: 0
LQI: 3
RSSI: -20
Done
```

### childmax

Get the Thread maximum number of allowed children.

```bash
> childmax
5
Done
```

### childmax \<count\>

Set the Thread maximum number of allowed children.

```bash
> childmax 2
Done
```

### childtimeout

Get the Thread Child Timeout value.

```bash
> childtimeout
300
Done
```

### childtimeout \<timeout\>

Set the Thread Child Timeout value.

```bash
> childtimeout 300
Done
```

### contextreusedelay

Get the CONTEXT_ID_REUSE_DELAY value.

```bash
> contextreusedelay
11
Done
```

### contextreusedelay \<delay\>

Set the CONTEXT_ID_REUSE_DELAY value.

```bash
> contextreusedelay 11
Done
```

### counter

Get the supported counter names.

```bash
>counter
mac
Done
```

### counter \<countername\>

Get the counter value.

```bash
>counter mac
TxTotal: 10
    TxUnicast: 3
    TxBroadcast: 7
    TxAckRequested: 3
    TxAcked: 3
    TxNoAckRequested: 7
    TxData: 10
    TxDataPoll: 0
    TxBeacon: 0
    TxBeaconRequest: 0
    TxOther: 0
    TxRetry: 0
    TxErrCca: 0
RxTotal: 2
    RxUnicast: 1
    RxBroadcast: 1
    RxData: 2
    RxDataPoll: 0
    RxBeacon: 0
    RxBeaconRequest: 0
    RxOther: 0
    RxWhitelistFiltered: 0
    RxDestAddrFiltered: 0
    RxDuplicated: 0
    RxErrNoFrame: 0
    RxErrNoUnknownNeighbor: 0
    RxErrInvalidSrcAddr: 0
    RxErrSec: 0
    RxErrFcs: 0
    RxErrOther: 0
```

### dataset help

Print meshcop dataset help menu.

```bash
> dataset help
help
active
activetimestamp
channel
channelmask
clear
commit
delay
extpanid
masterkey
meshlocalprefix
mgmtgetcommand
mgmtsetcommand
networkname
panid
pending
pendingtimestamp
pskc
securitypolicy
Done
>
```

### dataset active

Print meshcop active operational dataset.

```bash
> dataset active
Active Timestamp: 0
Done
```

### dataset activetimestamp \<activetimestamp\>

Set active timestamp.

```bash
> dataset activestamp 123456789
Done
```

### dataset channel \<channel\>

Set channel.

```bash
> dataset channel 12
Done
```

### dataset channelmask \<channelmask\>

Set channel mask.

```bash
> dataset channelmask e0ff1f00
Done
```

### dataset clear

Reset operational dataset buffer.

```bash
> dataset clear
Done
```

### dataset commit \<dataset\>

Commit operational dataset buffer to active/pending operational dataset.

```bash
> dataset commit active
Done
```

### dataset delay \<delay\>

Set delay timer value.

```bash
> dataset delay 1000
Done
```

### dataset extpanid \<extpanid\>

Set extended panid.

```bash
> dataset extpanid 000db80123456789
Done
```

### dataset masterkey \<masterkey\>

Set master key.

```bash
> dataset master 1234567890123456
Done
```

### dataset meshlocalprefix \<meshlocalprefix\>

Set mesh local prefix.

```bash
> dataset meshlocalprefix fd00:db8::
Done
```

### dataset mgmtgetcommand active \[address \<destination\>\] \[TLVs list\] \[binary\]

Send MGMT_ACTIVE_GET.

```bash
> dataset mgmtgetcommand active address fdde:ad00:beef:0:558:f56b:d688:799 activetimestamp 123 binary 0001
Done
```

### dataset mgmtsetcommand active \[TLV Types list\] \[binary\]

Send MGMT_ACTIVE_SET.

```bash
> dataset mgmtsetcommand active activetimestamp binary 820155
Done
```

### dataset mgmtgetcommand pending \[address \<destination\>\] \[TLVs list\] \[binary\]

Send MGMT_PENDING_GET.

```bash
> dataset mgmtgetcommand pending address fdde:ad00:beef:0:558:f56b:d688:799 activetimestamp binary 0001
Done
```

### dataset mgmtsetcommand pending \[TLV Types list\] \[binary\]

Send MGMT_PENDING_SET.

```bash
> dataset mgmtsetcommand pending activetimestamp binary 820155
Done
```

### dataset networkname \<networkname\>

Set network name.

```bash
> dataset networkname openthread
Done
```

### dataset panid \<panid\>

Set panid.

```bash
> dataset panid 0x1234
Done
```

### dataset pending

Print meshcop pending operational dataset.

```bash
> dataset pending
Done
```

### dataset pendingtimestamp \<pendingtimestamp\>

Set pending timestamp.

```bash
> dataset pendingtimestamp 123456789
Done
```

### dataset pskc \<pskc\>

Set pskc with hex format.

```bash
> dataset pskc 67c0c203aa0b042bfb5381c47aef4d9e
Done
```

### dataset securitypolicy \<rotationtime\> \[onrcb\]

Set security policy.

* o: Obtaining the Master Key for out-of-band commissioning is enabled.
* n: Native Commissioning using PSKc is allowed.
* r: Thread 1.x Routers are enabled.
* c: External Commissioner authentication is allowed using PSKc.
* b: Thread 1.x Beacons are enabled.

```bash
> dataset securitypolicy 672 onrcb
Done
```

### delaytimermin

Get the minimal delay timer (in seconds).

```bash
> delaytimermin
30
Done
```

### delaytimermin \<delaytimermin\>

Set the minimal delay timer (in seconds).

```bash
> delaytimermin 60
Done
```

### discover \[channel\]

Perform an MLE Discovery operation.

* channel: The channel to discover on.  If no channel is provided, the discovery will cover all valid channels.

```bash
> discover
| J | Network Name     | Extended PAN     | PAN  | MAC Address      | Ch | dBm | LQI |
+---+------------------+------------------+------+------------------+----+-----+-----+
| 0 | OpenThread       | dead00beef00cafe | ffff | f1d92a82c8d8fe43 | 11 | -20 |   0 |
Done
```

### dns resolve \<hostname\> \[DNS server IP\] \[DNS server port\]

Send DNS Query to obtain IPv6 address for given hostname.
The latter two parameters have following default values:
 * DNS server IP: 2001:4860:4860::8888 (Google DNS Server)
 * DNS server port: 53

```bash
> dns resolve ipv6.google.com
> DNS response for ipv6.google.com - [2a00:1450:401b:801:0:0:0:200e] TTL: 300
```

### eidcache

Print the EID-to-RLOC cache entries.

```bash
> eidcache
fdde:ad00:beef:0:bb1:ebd6:ad10:f33 ac00
fdde:ad00:beef:0:110a:e041:8399:17cd 6000
Done
```

### eui64

Get the factory-assigned IEEE EUI-64.

```bash
> eui64
0615aae900124b00
Done
```

### extaddr

Get the IEEE 802.15.4 Extended Address.

```bash
> extaddr
dead00beef00cafe
Done
```

### extaddr \<extaddr\>

Set the IEEE 802.15.4 Extended Address.

```bash
> extaddr dead00beef00cafe
dead00beef00cafe
Done
```

### extpanid

Get the Thread Extended PAN ID value.

```bash
> extpanid
dead00beef00cafe
Done
```

### extpanid \<extpanid\>

Set the Thread Extended PAN ID value.

```bash
> extpanid dead00beef00cafe
Done
```

### factoryreset
Delete all stored settings, and signal a platform reset.

```bash
> factoryreset
```

### hashmacaddr

Get the HashMac address.

```bash
> hashmacaddr
e0b220eb7d8dda7e
Done
```

### ifconfig

Show the status of the IPv6 interface.

```bash
> ifconfig
down
Done
```

### ifconfig up

Bring up the IPv6 interface.

```bash
> ifconfig up
Done
```

### ifconfig down

Bring down the IPv6 interface.

```bash
> ifconfig down
Done
```

### ipaddr

List all IPv6 addresses assigned to the Thread interface.

```bash
> ipaddr
fdde:ad00:beef:0:0:ff:fe00:0
fdde:ad00:beef:0:558:f56b:d688:799
fe80:0:0:0:f3d9:2a82:c8d8:fe43
Done
```

### ipaddr add \<ipaddr\>

Add an IPv6 address to the Thread interface.

```bash
> ipaddr add 2001::dead:beef:cafe
Done
```

### ipaddr del \<ipaddr\>

Delete an IPv6 address from the Thread interface.

```bash
> ipaddr del 2001::dead:beef:cafe
Done
```

### ipmaddr

List all IPv6 multicast addresses subscribed to the Thread interface.

```bash
> ipmaddr
ff05:0:0:0:0:0:0:1
ff33:40:fdde:ad00:beef:0:0:1
ff32:40:fdde:ad00:beef:0:0:1
Done
```

### ipmaddr add \<ipaddr\>

Subscribe the Thread interface to the IPv6 multicast address.

```bash
> ipmaddr add ff05::1
Done
```

### ipmaddr del \<ipaddr\>

Unsubscribe the Thread interface to the IPv6 multicast address.

```bash
> ipmaddr del ff05::1
Done
```

### ipmaddr promiscuous

Get multicast promiscuous mode.

```bash
> ipmaddr promiscuous
Disabled
Done
```

### ipmaddr promiscuous enable

Enable multicast promiscuous mode.

```bash
> ipmaddr promiscuous enable
Done
```

### ipmaddr promiscuous disable

Disable multicast promiscuous mode.

```bash
> ipmaddr promiscuous disable
Done
```

### joiner start \<pskd\> \<provisioningUrl\>

Start the Joiner role.

* pskd: Pre-Shared Key for the Joiner.
* provisioningUrl: Provisioning URL for the Joiner (optional).

This command will cause the device to perform an MLE Discovery and
initiate the Thread Commissioning process.

```bash
> joiner start PSK
Done
```

### joiner stop

Stop the Joiner role.

```bash
> joiner stop
Done
```

### joinerport \<port\>

Set the Joiner port.

```bash
> joinerport 1000
Done
```

### keysequence counter

Get the Thread Key Sequence Counter.

```bash
> keysequence counter
10
Done
```

### keysequence counter \<counter\>

Set the Thread Key Sequence Counter.

```bash
> keysequence counter 10
Done
```

### keysequence guardtime

Get Thread Key Switch Guard Time (in hours)

```bash
> keysequence guardtime
0
Done
```

### keysequence guardtime \<guardtime\>

Set Thread Key Switch Guard Time (in hours)
0 means Thread Key Switch imediately if key index match

```bash
> keysequence guardtime 0
Done
```

### leaderpartitionid

Get the Thread Leader Partition ID.

```bash
> leaderpartitionid
4294967295
Done
```

### leaderpartitionid \<partitionid\>

Set the Thread Leader Partition ID.

```bash
> leaderpartitionid 0xffffffff
Done
```

### leaderdata

Show the Thread Leader Data.

```bash
> leaderdata
Partition ID: 1077744240
Weighting: 64
Data Version: 109
Stable Data Version: 211
Leader Router ID: 60
Done
```

### leaderweight

Get the Thread Leader Weight.

```bash
> leaderweight
128
Done
```

### leaderweight \<weight\>

Set the Thread Leader Weight.

```bash
> leaderweight 128
Done
```

### linkquality \<extaddr\>

Get the link quality on the link to a given extended address.

```bash
> linkquality 36c1dd7a4f5201ff
3
Done
```

### linkquality \<extaddr\> \<linkquality\>

Set the link quality on the link to a given extended address.

```bash
> linkquality 36c1dd7a4f5201ff 3
Done
```

### masterkey

Get the Thread Master Key value.

```bash
> masterkey
00112233445566778899aabbccddeeff
Done
```

### masterkey \<key\>

Set the Thread Master Key value.

```bash
> masterkey 00112233445566778899aabbccddeeff
Done
```

### mode

Get the Thread Device Mode value.

* r: rx-on-when-idle
* s: Secure IEEE 802.15.4 data requests
* d: Full Function Device
* n: Full Network Data

```bash
> mode
rsdn
Done
```

### mode [rsdn]

Set the Thread Device Mode value.

* r: rx-on-when-idle
* s: Secure IEEE 802.15.4 data requests
* d: Full Function Device
* n: Full Network Data

```bash
> mode rsdn
Done
```

### netdataregister

Register local network data with Thread Leader.

```bash
> netdataregister
Done
```

### networkdiagnostic get \<addr\> \<type\> ..

Send network diagnostic request to retrieve tlv of \<type\>s.

If \<addr\> is unicast address, `Diagnostic Get` will be sent.
if \<addr\> is multicast address, `Diagnostic Query` will be sent.

```bash
> networkdiagnostic get fdde:ad00:beef:0:0:ff:fe00:f400 0 1 6
DIAG_GET.rsp: 00088e18ad17a24b0b740102f400060841dcb82d40bac63d

> networkdiagnostic get ff02::1 0 1
DIAG_GET.rsp: 0008567e31a79667a8cc0102f000
DIAG_GET.rsp: 0008aaa7e584759e4e6401025400
```

### networkdiagnostic reset \<addr\> \<type\> ..

Send network diagnostic request to reset \<addr\>'s tlv of \<type\>s. Currently only `MAC Counters`(9) is supported.

```bash
> diagnostic reset fd00:db8::ff:fe00:0 9
Done
```

### networkidtimeout

Get the NETWORK_ID_TIMEOUT parameter used in the Router role.  

```bash
> networkidtimeout
120
Done
```

### networkidtimeout \<timeout\>

Set the NETWORK_ID_TIMEOUT parameter used in the Router role.  

```bash
> networkidtimeout 120
Done
```

### networkname

Get the Thread Network Name.  

```bash
> networkname
OpenThread
Done
```

### networkname \<name\>

Set the Thread Network Name.  

```bash
> networkname OpenThread
Done
```

### panid

Get the IEEE 802.15.4 PAN ID value.

```bash
> panid
0xdead
Done
```

### panid \<panid\>

Set the IEEE 802.15.4 PAN ID value.

```bash
> panid 0xdead
Done
```

### parent

Get the diagnostic information for a Thread Router as parent.

```bash
> parent
Ext Addr: be1857c6c21dce55
Rloc: 5c00
Done
```

### ping \<ipaddr\> [size] [count] [interval]

Send an ICMPv6 Echo Request.

```bash
> ping fdde:ad00:beef:0:558:f56b:d688:799
16 bytes from fdde:ad00:beef:0:558:f56b:d688:799: icmp_seq=1 hlim=64 time=28ms
```

### pollperiod

Get the customized data poll period of sleepy end device (seconds). Only for certification test

```bash
> pollperiod
0
Done
```

### pollperiod \<pollperiod\>

Set the customized data poll period for sleepy end device (seconds). Only for certification test

```bash
> pollperiod 10
Done
```

### prefix add \<prefix\> [pvdcsr] [prf]

Add a valid prefix to the Network Data.

* p: Preferred flag
* a: Stateless IPv6 Address Autoconfiguration flag
* d: DHCPv6 IPv6 Address Configuration flag
* c: DHCPv6 Other Configuration flag
* r: Default Route flag
* o: On Mesh flag
* s: Stable flag
* prf: Default router preference, which may be 'high', 'med', or 'low'.

```bash
> prefix add 2001:dead:beef:cafe::/64 paros med
Done
```

### prefix remove \<prefix\>

Invalidate a prefix in the Network Data.

```bash
> prefix remove 2001:dead:beef:cafe::/64
Done
```

### promiscuous

Get radio promiscuous property.

```bash
> promiscuous
Disabled
Done
```

### promiscuous enable

Enable radio promiscuous operation and print raw packet content.

```bash
> promiscuous enable
Done
```

### promiscuous disable

Disable radio promiscuous operation.

```bash
> promiscuous disable
Done
```

### releaserouterid \<routerid\>
Release a Router ID that has been allocated by the device in the Leader role.

```bash
> releaserouterid 16
Done
```

### reset
Signal a platform reset.

```bash
> reset
```

### rloc16

Get the Thread RLOC16 value.

```bash
> rloc16
0xdead
Done
```

### route add \<prefix\> [s] [prf]

Add a valid prefix to the Network Data.

* s: Stable flag
* prf: Default Router Preference, which may be: 'high', 'med', or 'low'.

```bash
> route add 2001:dead:beef:cafe::/64 s med
Done
```

### route remove \<prefix\>

Invalidate a prefix in the Network Data.

```bash
> route remove 2001:dead:beef:cafe::/64
Done
```

### router list

List allocated Router IDs.

```bash
> router list
8 24 50
Done
```

### router table

Print table of routers.

```bash
> router table
| ID | RLOC16 | Next Hop | Path Cost | LQI In | LQI Out | Age | Extended MAC     |
+----+--------+----------+-----------+--------+---------+-----+------------------+
| 21 | 0x5400 |       21 |         0 |      3 |       3 |   5 | d28d7f875888fccb |
| 56 | 0xe000 |       56 |         0 |      0 |       0 | 182 | f2d92a82c8d8fe43 |
Done
```

### router \<id\>

Print diagnostic information for a Thread Router.  The `id` may be a Router ID or an RLOC16.

```bash
> router 50
Alloc: 1
Router ID: 50
Rloc: c800
Next Hop: c800
Link: 1
Ext Addr: e2b3540590b0fd87
Cost: 0
LQI In: 3
LQI Out: 3
Age: 3
Done
```

```bash
> router 0xc800
Alloc: 1
Router ID: 50
Rloc: c800
Next Hop: c800
Link: 1
Ext Addr: e2b3540590b0fd87
Cost: 0
LQI In: 3
LQI Out: 3
Age: 7
Done
```

### routerdowngradethreshold

Get the ROUTER_DOWNGRADE_THRESHOLD value.

```bash
> routerdowngradethreshold
23
Done
```

### routerdowngradethreshold \<threshold\>

Set the ROUTER_DOWNGRADE_THRESHOLD value.

```bash
> routerdowngradethreshold 23
Done
```

### routerrole

Indicates whether the router role is enabled or disabled.

```bash
> routerrole
Enabled
Done
```

### routerrole enable

Enable the router role.

```bash
> routerrole enable
Done
```

### routerrole disable

Disable the router role.

```bash
> routerrole disable
Done
```

### routerselectionjitter

Get the ROUTER_SELECTION_JITTER value.

```bash
> routerselectionjitter
120
Done
```

### routerselectionjitter \<jitter\>

Set the ROUTER_SELECTION_JITTER value.

```bash
> routerselectionjitter 120
Done
```

### routerupgradethreshold

Get the ROUTER_UPGRADE_THRESHOLD value.

```bash
> routerupgradethreshold
16
Done
```

### routerupgradethreshold \<threshold\>

Set the ROUTER_UPGRADE_THRESHOLD value.

```bash
> routerupgradethreshold 16
Done
```

### scan \[channel\]

Perform an IEEE 802.15.4 Active Scan.

* channel: The channel to scan on.  If no channel is provided, the active scan will cover all valid channels.

```bash
> scan
| J | Network Name     | Extended PAN     | PAN  | MAC Address      | Ch | dBm | LQI |
+---+------------------+------------------+------+------------------+----+-----+-----+
| 0 | OpenThread       | dead00beef00cafe | ffff | f1d92a82c8d8fe43 | 11 | -20 |   0 |
Done
```

### singleton
Return true when there are no other nodes in the network, otherwise return false.

```bash
> singleton
true or false
Done
```

### state
Return state of current state.

```bash
> state
offline, disabled, detached, child, router or leader
Done
```

### state <state>
Try to switch to state `detached`, `child`, `router` or `leader`.

```bash
> state leader
Done
```

### thread start

Enable Thread protocol operation and attach to a Thread network.

```bash
> thread start
Done
```

### thread stop

Disable Thread protocol operation and detach from a Thread network.

```bash
> thread stop
Done
```

### txpowermax

Get the maximum transmit power in dBm.

```bash
> txpowermax
-10 dBm
Done
```

### txpowermax \<txpowermax\>

Set the maximum transmit power.

```bash
> txpowermax -10
Done
```

### version

Print the build version information.

```bash
> version
OPENTHREAD/gf4f2f04; Jul  1 2016 17:00:09
Done
```

### whitelist

List the whitelist entries.

```bash
> whitelist
Enabled
e2b3540590b0fd87
d38d7f875888fccb
c467a90a2060fa0e
Done
```

### whitelist add \<extaddr\>

Add an IEEE 802.15.4 Extended Address to the whitelist.

```bash
> whitelist add dead00beef00cafe
Done
```

### whitelist clear

Clear all entries from the whitelist.

```bash
> whitelist clear
Done
```

### whitelist disable

Disable MAC whitelist filtering.

```bash
> whitelist disable
Done
```

### whitelist enable

Enable MAC whitelist filtering.

```bash
> whitelist enable
Done
```

### whitelist remove \<extaddr\>

Remove an IEEE 802.15.4 Extended Address from the whitelist.

```bash
> whitelist remove dead00beef00cafe
Done
```

## OpenThread Diagnostics Module Reference

The OpenThread diagnostics module is a tool for debugging platform hardware manually, and it will also be used during manufacturing process, to verify platform hardware performance.

The diagnostics module supports common diagnostics features that are listed below, and it also provides a mechanism for expanding platform specific diagnostics features.


## Common Diagnostics Command List

* [diag](#diag)
* [diag start](#diag-start)
* [diag channel](#diag-channel)
* [diag power](#diag-power)
* [diag send](#diag-send-packets-length)
* [diag repeat](#diag-repeat-delay-length)
* [diag sleep](#diag-sleep)
* [diag stats](#diag-stats)
* [diag stop](#diag-stop)


### diag

Show diagnostics mode status.

```bash
> diag
diagnostics mode is disabled
```

### diag start

Start diagnostics mode.

```bash
> diag start
start diagnostics mode
status 0x00
```

### diag channel

Get the IEEE 802.15.4 Channel value for diagnostics module.

```bash
> diag channel
channel: 11
```

### diag channel \<channel\>

Set the IEEE 802.15.4 Channel value for diagnostics module.

```bash
> diag channel 11
set channel to 11
status 0x00
```

### diag power

Get the tx power value(dBm) for diagnostics module.

```bash
> diag power
tx power: -10 dBm
```

### diag power \<power\>

Set the tx power value(dBm) for diagnostics module.

```bash
> diag power -10
set tx power to -10 dBm
status 0x00
```

### diag send \<packets\> \<length\>

Transmit a fixed number of packets with fixed length.

```bash
> diag send 20 100
sending 0x14 packet(s), length 0x64
status 0x00
```

### diag repeat \<delay\> \<length\>

Transmit packets repeatedly with a fixed interval.

```bash
> diag repeat 100 100
sending packets of length 0x64 at the delay of 0x64 ms
status 0x00
```

### diag repeat stop

Stop repeated packet transmission.

```bash
> diag repeat stop
repeated packet transmission is stopped
status 0x00
```

### diag sleep

Enter radio sleep mode.

```bash
> diag sleep
sleeping now...
```

### diag stats

Print statistics during diagnostics mode.

```bash
> diag stats
received packets: 10
sent packets: 10
first received packet: rssi=-65, lqi=101
```

### diag stop

Stop diagnostics mode and print statistics.

```bash
> diag stop
received packets: 10
sent packets: 10
first received packet: rssi=-65, lqi=101

stop diagnostics mode
status 0x00
```
## Platform based diag commands

* [diag transmit](#diag-transmit)
* [diag listen](#diag-listen)
* [diag id](#diag-id)

### diag transmit

Transmit a packet in diag mode

#### Set the count of transmission
```bash
209071 > compat> ot diag transmit count 10
210515 diag transmit count 10
210515 set diagnostic messages count to 10
210515 status 0x00
```
#### Set the interval of transmission
```bash
210515 > compat> ot diag transmit interval 100
211715 diag transmit interval 100
211715 set diagnostic messages interval to 100 ms
211716 status 0x00
211716
```
#### Start the transmission
```bash
211716 > compat> ot diag transmit start
212790 diag transmit start
212790 sending 10 diagnostic messages with 100 ms interval
212790 status 0x00
212790
212790 > compat> Transmit done
```

### diag listen

Listen to diag packets

```bash
200855 > compat> ot diag listen 1
205494 diag listen 1
205494 set listen to yes
205494 status 0x00

205494 > compat> ot diag listen 0
206350 diag listen 0
206350 set listen to no
206350 status 0x00
```
When the first device does a "diag transmit start", the listening device will get following logs
```bash
238974 compat> {"Frame":{"LocalChannel":5 ,"RemoteChannel":20,"CNT":10,"LocalID":100,"RemoteID":20,"RSSI":0}}
{"Frame":{"LocalChannel":5 ,"RemoteChannel":20,"CNT":11,"LocalID":100,"RemoteID":20,"RSSI":0}}
{"Frame":{"LocalChannel":5 ,"RemoteChannel":20,"CNT":12,"LocalID":100,"RemoteID":20,"RSSI":0}}
{"Frame":{"LocalChannel":5 ,"RemoteChannel":20,"CNT":13,"LocalID":100,"RemoteID":20,"RSSI":0}}
{"Frame":{"LocalChannel":5 ,"RemoteChannel":20,"CNT":14,"LocalID":100,"RemoteID":20,"RSSI":0}}
{"Frame":{"LocalChannel":5 ,"RemoteChannel":20,"CNT":15,"LocalID":100,"RemoteID":20,"RSSI":0}}
{"Frame":{"LocalChannel":5 ,"RemoteChannel":20,"CNT":16,"LocalID":100,"RemoteID":20,"RSSI":0}}
{"Frame":{"LocalChannel":5 ,"RemoteChannel":20,"CNT":17,"LocalID":100,"RemoteID":20,"RSSI":0}}
{"Frame":{"LocalChannel":5 ,"RemoteChannel":20,"CNT":18,"LocalID":100,"RemoteID":20,"RSSI":0}}
{"Frame":{"LocalChannel":5 ,"RemoteChannel":20,"CNT":19,"LocalID":100,"RemoteID":20,"RSSI":0}}
```

### diag id

Assign an ID to the device

```bash
237867 > compat> ot diag id 100
238772 diag id 100
238772 set ID to 100
238772 status 0x00
```
