#!/usr/bin/env python
import sys, argparse
import numpy as np
import math
import matplotlib.pyplot as plt
import matplotlib.patches as patches
import json

th = 0.01
radconv=180.0/np.pi
_bins = 10
_normalize = False

def interpret_status(status):
    print("Status " + status + ":")
    s = int(status,16)
    if (s & 0x00000001): print("  0x00000001 Interrupt Request Status READ ONLY");
    if (s & 0x00000002): print("  0x00000002 Clock PLL Lock");
    if (s & 0x00000004): print("  0x00000004 External Sync Clock Reset");
    if (s & 0x00000008): print("  0x00000008 Automatic Acknowledge Trigger");
    if (s & 0x00000010): print("  0x00000010 Transmit Frame Begins");
    if (s & 0x00000020): print("  0x00000020 Transmit Preamble Sent");
    if (s & 0x00000040): print("  0x00000040 Transmit PHY Header Sent ");
    if (s & 0x00000080): print("  0x00000080 Transmit Frame Sent: This is set when the transmitter has completed the sending of a frame ");
    if (s & 0x00000100): print("  0x00000100 Receiver Preamble Detected status ");
    if (s & 0x00000200): print("  0x00000200 Receiver Start Frame Delimiter Detected. ");
    if (s & 0x00000400): print("  0x00000400 LDE processing done ");
    if (s & 0x00000800): print("  0x00000800 Receiver PHY Header Detect ");
    if (s & 0x00001000): print("  0x00001000 Receiver PHY Header Error ");
    if (s & 0x00002000): print("  0x00002000 Receiver Data Frame Ready ");
    if (s & 0x00004000): print("  0x00004000 Receiver FCS Good ");
    if (s & 0x00008000): print("  0x00008000 Receiver FCS Error ");
    if (s & 0x00010000): print("  0x00010000 Receiver Reed Solomon Frame Sync Loss ");
    if (s & 0x00020000): print("  0x00020000 Receive Frame Wait Timeout ");
    if (s & 0x00040000): print("  0x00040000 Leading edge detection processing error ");
    if (s & 0x00080000): print("  0x00080000 bit19 reserved ");
    if (s & 0x00100000): print("  0x00100000 Receiver Overrun ");
    if (s & 0x00200000): print("  0x00200000 Preamble detection timeout ");
    if (s & 0x00400000): print("  0x00400000 GPIO interrupt ");
    if (s & 0x00800000): print("  0x00800000 SLEEP to INIT ");
    if (s & 0x01000000): print("  0x01000000 RF PLL Losing Lock ");
    if (s & 0x02000000): print("  0x02000000 Clock PLL Losing Lock ");
    if (s & 0x04000000): print("  0x04000000 Receive SFD timeout ");
    if (s & 0x08000000): print("  0x08000000 Half Period Delay Warning ");
    if (s & 0x10000000): print("  0x10000000 Transmit Buffer Error ");
    if (s & 0x20000000): print("  0x20000000 Automatic Frame Filtering rejection ");
    if (s & 0x40000000): print("  0x40000000 Host Side Receive Buffer Pointer ");
    if (s & 0x80000000): print("  0x80000000 IC side Receive Buffer Pointer READ ONLY ");
    if (s & 0x0100000000): print("  0x0100000000 Receiver Reed-Solomon Correction Status");
    if (s & 0x0200000000): print("  0x0200000000 Receiver Preamble Rejection");
    if (s & 0x0400000000): print("  0x0400000000 Transmit power up time error" );
    
    

def packet_length_symb(data_len, datarate, preamble_len, prf):
    msgdataleni = data_len
    preambleleni = 0;
    sfdlen = 0;
    x = 0;
    
    xi = np.ceil(msgdataleni*8/330.0)
    msgdataleni = msgdataleni*8 + xi*48 

    #assume PHR length is 172308ns for 110k and 21539ns for 850k/6.81M
    #SFD length is 64 for 110k (always)
    #SFD length is 8 for 6.81M, and 16 for 850k, but can vary between 8 and 16 bytes
    if(datarate == 110):
	msgdataleni *= 8205.13;
	msgdataleni += 172308;
        sfdlen = 64;
    elif(datarate == 850):
	msgdataleni *= 1025.64;
	msgdataleni += 21539;
	sfdlen = 16;
    else:
	msgdataleni *= 128.21;
	msgdataleni += 21539;
	sfdlen = 8;
        
    preambleleni = preamble_len

    #preamble  = plen * (994 or 1018) depending on 16 or 64 PRF
    if(prf == 16):
	preambleleni = (sfdlen + preambleleni) * 0.99359
    else:
	preambleleni = (sfdlen + preambleleni) * 1.01763
	
    #set the frame wait timeout time - total time the frame takes in symbols
    # 16 - receiver startup margin
    preamble_sy = preambleleni / 1.0256;
    data_sy	    = (msgdataleni/1000.0)/1.0256;
    total_sy    = preamble_sy + data_sy #+ 16/1.0256 + 

    return [preamble_sy,data_sy,total_sy];
    
def plotHistogram(data, args, label=None):
    fig = plt.figure(num=1,figsize=(10,10))
    ax = fig.add_subplot(111) #, aspect='equal'
    plt.tight_layout()
    ax.grid()
    field = 'pd'
    l = args.split(',')
    if (len(l)>0):
        field=l[0]
    pdata = []
    for x in data:
        try:
            pdata.append(float(x[field])*180.0/np.pi)
        except:
            pass
    stats = "Histogram: records: {:d} average: {:.3f} stddev:  {:.3f}".format(len(pdata), np.mean(pdata), np.std(pdata))    
    print(stats)
    plt.hist(pdata, bins=96, normed=0)
    if (label):
        plt.title(label + " " + stats)
    
def main():
    global _bins, _normalize
    parser = argparse.ArgumentParser()
    parser.add_argument('files', metavar='file[s]', nargs='*', help='json files to plot')
    parser.add_argument('-r',metavar='prf',dest='prf',default=64,type=int,help='prf, 16 or 64(default)')
    parser.add_argument('-q',metavar='plen',dest='preamble_len',default=128,type=int,help='Preamble length (default=128)')
    parser.add_argument('-l',metavar='message_length',type=int,dest='data_len',help='Number of bytes in message')
    parser.add_argument('-b',metavar='data_rate',type=int,dest='data_rate',default=6800,help='Data rate, 110, 850 or 6800(default)')
    parser.add_argument('-s',metavar='status',dest='status',default='',help='Interpret status register')
    parser.add_argument('-p', dest='plot', action='store_true',help='generate a plot')
    parser.add_argument('--histogram', metavar='field[,bins]', type=str, help='generate a histogram')
    parser.add_argument('--plot_label', metavar='title', type=str, help='Label to apply to plot')
    parser.add_argument('--save-plot', dest='save_plot', metavar='filename', type=str, help='file to save plot to (png)')
    parser.add_argument('-v', dest='verbose', action='store_true',help='verbose')
    args = parser.parse_args()
    
    prf=64
    data_len=32
    data_rate=6800
    preamble_len=128
    if (args.prf): prf = args.prf
    if (args.data_len): data_len = args.data_len
    if (args.data_rate): data_rate = args.data_rate
    if (args.preamble_len): preamble_len = args.preamble_len
    if (args.status):
        interpret_status(args.status)
        return
    
    [preamble_sy,data_sy,total_sy] = packet_length_symb(data_len,data_rate,preamble_len,prf)
    uus = (total_sy)*1.0256
    
    print("Prf:	   %3d"%(prf))
    print("Datarate: %4d"%(data_rate))
    print("Preamble:  %3d: %.3f sy"%(preamble_len,preamble_sy))
    print("Data:      %3d: %.3f sy"%(data_len,data_sy))
    print("===========================")
    print("Total:   %3d sy, us: %.3f\n"%(total_sy,uus))
    
    if len(args.files)<1: exit(0)
    
    data = []
    with open(args.files[0]) as f:
	for line in f:
	    try:
		data.append(json.loads(line))
	    except ValueError:
		pass


    if (args.histogram):
        plotHistogram(data, args.histogram, label=args.plot_label)
        if (args.save_plot):
            plt.savefig(args.save_plot, bbox_inches='tight')
        else:
            plt.show()
        sys.exit(0)
        
    if (args.plot):
        fig = plt.figure()
        ax = fig.add_subplot(111) #, aspect='equal'
        plt.tight_layout()
        ax.grid()

    tmin = 0;
    tmax = 0;
    text_y=10;
    t=0
    try:
        t0=(int(data[0]["ts"],16))
    except TypeError:
        t0=(data[0]["ts"])

    print("ts       margin  plen      [start, end] data(hex)")
    print("-----------------------------------------------------------------------")
    last_ts_end = 0
    for d in data:
        try:
            t=(int(d["ts"],16)-t0)/0xffff
        except TypeError:
            t=(d["ts"]-t0)/0xffff
        except KeyError:
            t=(int(d["ts0"],16)-t0)/0xffff

        data="(no data)"
        try:
            dlen = d["dlen"]
            data = d["d"]
        except KeyError:
            try:
                dlen = len(d["d"])
                data = d["d"]
            except KeyError:
                dlen = 0
        [preamble_sy,data_sy,total_sy] = packet_length_symb(dlen,data_rate,preamble_len,prf)
        delta_ts = 0
        if (last_ts_end > 0): delta_ts = t-preamble_sy - last_ts_end
        last_ts_end = t+data_sy
        print("%7d %7d %5d [%7d,%7d] %s"%(t, delta_ts, total_sy, t-preamble_sy, t+data_sy, data))

        if (args.plot) :
            # Preamble
            ax.add_patch(patches.Rectangle((t-preamble_sy, 0), preamble_sy, 10, fc='r'))
            ax.add_patch(patches.Rectangle((t, 0), data_sy, 10, fc='g'))
            ax.annotate(data, (t, text_y))
            text_y+=0.5
            if (text_y>12): text_y=10
            if (t-preamble_sy) < tmin: tmin = t-preamble_sy
            if (t+data_sy) > tmax: tmax = t+data_sy

    if (args.plot) :
        ax.axis([tmin-tmax*0.05,tmax*1.05, 0, 12.5])        
        plt.show()
                
if __name__ == "__main__":
    main()
