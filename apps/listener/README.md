# UWB sniffer / listener

## Overview
This example is really a very useful tool for assisting you develop UWB applications. It allows you to monitor the traffic and get accurate
timestamps for each package. 

### Building target for dwm1001

```no-highlight
newt target create dwm1001_listener
newt target set dwm1001_listener app=apps/listener
newt target set dwm1001_listener bsp=@decawave-uwb-core/hw/bsp/dwm1001
newt target amend dwm1001_listener syscfg=CONSOLE_UART_BAUD=115200
newt run dwm1001_listener 0
```

### Building listener for dwm3120 on ST nucleo

```no-highlight
newt target create dwm3120_listener
newt target set dwm3120_listener app=apps/listener
newt target set dwm3120_listener bsp=@decawave-uwb-core/hw/bsp/nucleo-f429zi
newt target amend dwm3120_listener syscfg=CONSOLE_UART_BAUD=115200:BLE_ENABLED=0
newt run dwm3120_listener 0
```


If you connect to the virtual serial port, on linux /dev/ttyACMx where x is a number assigned by your computer, you should
see json packaged coming similar to the ones below if you have UWB traffic within range of the dwm1001:
```
$ socat /dev/ttyACM0,b115200,raw,echo=0,nonblock $(tty),raw,echo=0,escape=0x03

{"utime":32240936,"ts":959949481290,"rssi":-84.0,"dlen":30,"d":"c5939204b2cf86c102049204000000000400000050c03791490713000004"}
{"utime":32509826,"ts":977129894189,"rssi":-84.1,"dlen":30,"d":"c5949204b2cf86c1020492040000000004000000500038914d0713000004"}
```

The fields are:

- utime: Microcontroller time, usec
- ts0: uwb timestamp
- rssi: estimated received signal strength in dBm
- dlen: length of data packet
- d: data in hexadecimal


### Building target for dwm1002

The dwm1002 has the advantage of having two receivers and can thus extract information about the phase difference to estimate
a direction to the incoming packet. In the example below all this data (and more) is included in the json stream.

```no-highlight
newt target create dwm1002_listener
newt target set dwm1002_listener app=apps/listener
newt target set dwm1002_listener bsp=@decawave-uwb-core/hw/bsp/dwm1002
newt target amend dwm1002_listener syscfg=CONSOLE_UART_BAUD=460800:UWB_DEVICE_0=1:UWB_DEVICE_1=1:CIR_ENABLED=1:USE_DBLBUFFER=0
newt run dwm1002_listener 0
```

If you connect to the virtual serial port, on linux /dev/ttyACMx where x is a number assigned by your computer, you should
see json packaged coming similar to the ones below if you have UWB traffic within range of the dwm1001:
```
$ socat /dev/ttyACM0,b460800,raw,echo=0,nonblock -

{"utime":3219787,"ts":[203624381252,203624381278],"rssi":[-80.5,-81.3],"fppl":[-84.5,-85.3],"pd":[1.267],"dlen":30,"d":"c5439204b2cf86c102049204000000000400000050c0639c09ba13000004","cir0":{"o":6,"fp_idx":745.187,"rcphase":1.030,"angle":2.588,"real":[92,2,79,127,-36,-429,-2656,-6108,-6876,-5448,1261,4428,1492,-1380,-1113,275],"imag":[-172,-82,37,61,35,232,1641,3090,2532,-642,-4520,-3425,470,2132,467,-806]},"cir1":{"o":6,"fp_idx":745.593,"rcphase":0.932,"angle":1.222,"real":[-164,1,111,36,16,619,1620,312,-3040,-3991,-948,1312,717,-467,-372,165],"imag":[-89,-140,-6,85,102,1132,4460,6440,5022,-1016,-4559,-1405,2121,1369,-1023,-1252]}}

```

The fields are:

- utime: Microcontroller time, usec
- ts: [uwb_0 timestamp
- ts1: uwb_1 timestamp
- rssi0: estimated received signal strength in dBm for receiver 0
- rssi1: estimated received signal strength in dBm for receiver 1
- pd: estimated phase difference in radians
- cir0: accumulator data for receiver 0
  - o: offset relative detected leading edge (how many samples before the edge to extract)
  - fp_idx: leading edge location in accumulator (floating point)
- cir1: accumulator data for receiver 1
- dlen: length of data packet
- d: data in hexadecimal

### Building target for ttk1000

The ttk1000 can broadcast the UWB results as UDP packets on the local network.

```no-highlight
newt target create ttk1000_listener
newt target set ttk1000_listener app=apps/listener
newt target set ttk1000_listener bsp=@decawave-uwb-core/hw/bsp/ttk1000
newt target amend ttk1000_listener syscfg=CONSOLE_UART_BAUD=460800:UWB_DEVICE_0=1:USE_DBLBUFFER=1
newt run ttk1000_listener 0
```
