README [LWIP PING TX/RX]
------------------------

* Applications
	-> mynewt-dw1000-apps/apps
				-> lwip_ping_tx : App to transmit PING packet.
				-> lwip_ping_rx : App to receive PING packet.

* Prepare the build
	1. git clone https://github.com/Decawave/mynewt-dw1000-apps.git
	2. git checkout master [default] && git pull
	3. newt install
		[Enter github credentials]
	4. cd repo/mynewt-dw1000-core
	5. git checkout master [default] && git pull
	6. cd ../../
		[Back to mynewt-dw1000-apps]


* Build and flash Bootloader
	[Refer: mynewt-dw1000-apps/README.md]

* Build and flash lwip_ping_tx in DEV-1
	1. cd <Path>/mynewt-dw1000-apps [if it is not the PWD]
	2. newt target create lwip_ping_tx
	3. newt target set lwip_ping_tx app=apps/lwip_ping_tx
	4. newt target set lwip_ping_tx bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
	5. newt target set lwip_ping_tx build_profile=debug
	6. newt build lwip_ping_tx
	7. newt create-image lwip_ping_tx 1.0.0
	8. newt load lwip_ping_tx

* Build and flash lwip_ping_rx in DEV-2
	1. cd <Path>/mynewt-dw1000-apps [if it is not the PWD]
	2. newt target create lwip_ping_rx
	3. newt target set lwip_ping_rx app=apps/lwip_ping_rx
	4. newt target set lwip_ping_rx bsp=@mynewt-dw1000-core/hw/bsp/dwm1001
	5. newt target set lwip_ping_rx build_profile=debug
	6. newt build lwip_ping_rx
	7. newt create-image lwip_ping_rx 1.0.0
	8. newt load lwip_ping_rx

* Now we have two modules flashed and configured as lwip PING Tx and Rx respectively.

1. Connect DEV-2 and open serial terminal for lwip_ping_rx 
	$ sudo minicom -D /dev/ttyACM0
2. Connect lwip_ping_tx device and open its serial terminal
	$ sudo minicom -D /dev/ttyACM1
3. Now, both TX and RX are connected to your PC.
4. Notice the following message

	[Ping Received]
	Seq # = <Sequence #>

   in the RX terminal, and

	[Ping Sent]
	Seq # - <Sequence #>

   in the TX terminal.
5. If the messages mentioned above are shown, then the PING happened successfully.


