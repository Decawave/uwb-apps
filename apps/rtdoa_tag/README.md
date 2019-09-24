# RTDoA

## To upload a new firmware through BLE:

1.	Download the new firmware (file ending in .img) onto your iphone/ipad and save it in “Files”.
    Making sure to change the file-suffix to either txt or png (otherwise ios will not show it)
    You can do this by either emailing it to yourself and on the phone saving it to Files or using airdrop. 
2.	open the Mynewt Manager app and connect to the board of choice. 
3.	enter the Images tab and select “Upload a custom image”
4.	select browse and navigate to the file you saved in step 1. For example rtdoa_node_0_1_0_1.txt.
    Uploading will take a minute or two.
5.	once uploaded, the Image slots will update with the new image you uploaded. 
6.	select the new image in the list and click “Test”. The app will ask you for confirmation and reset the board.
    The board will disconnect.
7.	the board will now swap the two images. Note, this will take 20-50s. 
8.	reconnect to the board and enter the images tab again. 
9.	if successful, select the image with the new firmware, shown as “Main slot – Testing” and select confirm.
    This will ensure that the board will not swap back to the old firmware next time it reboots.
    The board will reset again and swap the two images if there's a problem.

Note that if the update fails for some reason the board will revert to the old firmware. 

## Serial Connection

The board comes up as a USB serial port. The settings you need to use are 1000000bps 8-n-1.

## Creating a master node

A rtdoa network needs a master node and 1 or more slave nodes. All nodes are by default setup as slaves.
To configure a node as a master you connect over the serial port and issue the following commands:

```

config ntwr/role 0x1
config save
config commit

```

You can then check that it worked by using the following command:

```
config dump
```

which will give you a whole list of parameters. Look and verify that the ntwr/role is set to 1 or 0x1.

NOTE: Only one node can be master, the others have to be slaves (0x0 or 0)

## Listing nodes in the current network

On the master node, issue the command ```panm list``` over the usb. A list of the nodes connected to this master will show.
If using the command ```tdb list``` instead, a list of nodes and their distance to the master should show. 

## Tag data

The tag outputs data on it's local usb in json format like below:

{"id":"0x499b","ts":"2473.4125","msgid":50196,"vbat":"4.29","usb":1,
"acc":"[0.254,-8.345,-5.99]","gyro":"[0.5,0.3,0.4]","mag":[-548,140,-373],"pres":102072,
"ref_anchor":"492","meas":[{"addr":"492","ddist":"0.005","tqf":1,"rssi":"-82.8"},{"addr":"533","ddist":"0.698","tqf":1,"rssi":"-79.9"}]}

The fields are:
  id       The local tag's own 16-bit id
  ts       Timestamp relative the master node.
  msgid    A number increasing with every message sent until the board resets
  vbat     If a lipo is connected this field will show it's voltage level otherwise it'll show a value of about 4.30V.
  usb      1 indicates that the board has usb power
  acc      Accelerometer (m/s^2)
  gyro     dps (Degrees per second)
  mag      Magnetometer / Compass (uT)
  pres     Air-pressure in Pa
  ref_anchor The reference anchor used for the rtdoa sequence. This is the base for the difference of distances.
  meas     Vector of rtdoa measurements
    addr   Address of node
    ddist  difference in distance between this node and the reference anchor
    tfq    quality of the measurement (only 1 or 0 at the moment, estimation of Line of Sight)
    rssi   Receiver strength of signal from this anchor

