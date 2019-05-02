#!/usr/bin/env python

import sys, argparse
import socket
import serial
import json
import struct
import numpy as np
import time
import multiprocessing
import matplotlib
matplotlib.use('GTKAgg')
from matplotlib import pyplot as plt

from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2TkAgg
from matplotlib.figure import Figure
import matplotlib.animation as animation
from matplotlib import style
style.use('ggplot')
        
import tkinter as tk
from tkinter import ttk

LARGE_FONT= ("Verdana", 12)


class PdoaApp(tk.Tk):

    def __init__(self, data_feed=None):
        
        tk.Tk.__init__(self)  #, *args, **kwargs

        #tk.Tk.iconbitmap(self, default="clienticon.ico")
        tk.Tk.wm_title(self, "Pdoa visualisation")
        
        #Create a queue to share data between process
        q = multiprocessing.Queue()
        
        #Create and start the datafeed process
        self.input_data = multiprocessing.Process(None, input_thread, args=(q, data_feed,))
        self.input_data.start()
    
        container = tk.Frame(self)
        container.pack(side="top", fill="both", expand = True)
        container.grid_rowconfigure(0, weight=1)
        container.grid_columnconfigure(0, weight=1)

        frame = CirPlots(container, self, data_queue=q)
        frame.grid(row=0, column=0, sticky="nsew")
        frame.tkraise()

    def close(self):
        print("quitting")
        self.input_data.terminate()
        self.destroy()
        exit(0)

class CirPlots(tk.Frame):

    def __init__(self, parent, controller, data_queue=None):
        self.idx = 0
        self.cir_ymin = -1000
        self.cir_ymax = 1000
        self.tic = time.time()

        self.parent = parent;
        tk.Frame.__init__(self, parent)
        # 
        self.grid_rowconfigure(1, weight=1)
        self.grid_columnconfigure(0, weight=1)
        
        self.top_frame = tk.Frame(self, bg='cyan', pady=3)
        self.top_frame.grid(row=0, sticky="ew")
        self.top_frame.grid_rowconfigure(0, weight=1)
        self.top_frame.grid_columnconfigure(1, weight=1)
        quit_btn = tk.Button(self.top_frame, text="QUIT", fg="red",
                              command=controller.close)
        quit_btn.grid(row=0, column=0)
        reset_btn = tk.Button(self.top_frame, text="RESET", fg="black",
                              command=lambda: self.resetplot())
        reset_btn.grid(row=0, column=1)
        self.queue_stats = tk.Label(self.top_frame, text="stats", font=("Verdana", 10))
        self.queue_stats.grid(row=0, column=2, sticky="w")

        self.center_frame = tk.Frame(self, bg='gray2', pady=1)
        self.center_frame.grid(row=1, sticky="sw")
        self.center_frame.grid_rowconfigure(0, weight=1)
        self.center_frame.grid_columnconfigure(1, weight=1)
        #self.center_frame.pack(fill=tk.BOTH, expand=True)

        self.left_frame = tk.Frame(self.center_frame, bg='white', pady=3)
        self.left_frame.grid(row=0, column=0, sticky="se")
        
        self.fig = Figure(figsize=(6,6), dpi=100)
        self.a0 = self.fig.add_subplot(211)
        self.a1 = self.fig.add_subplot(212)
        self.canvas = FigureCanvasTkAgg(self.fig, self.left_frame)
        self.canvas.show()
        self.canvas.get_tk_widget().pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True)
        self.line_0r, = self.a0.plot(xrange(16), 8*[self.cir_ymin, self.cir_ymax], 'r')
        self.line_0i, = self.a0.plot(xrange(16), 8*[self.cir_ymin, self.cir_ymax], 'b')
        self.line_0a, = self.a0.plot(xrange(16), 8*[self.cir_ymin, self.cir_ymax], 'k', linewidth=2.0)
        self.line_0fp, = self.a0.plot([0,0], [-100,100], 'g--', linewidth=2.0)
        self.line_1r, = self.a1.plot(xrange(16), 8*[self.cir_ymin, self.cir_ymax], 'r')
        self.line_1i, = self.a1.plot(xrange(16), 8*[self.cir_ymin, self.cir_ymax], 'b')
        self.line_1a, = self.a1.plot(xrange(16), 8*[self.cir_ymin, self.cir_ymax], 'k', linewidth=2.0)
        self.line_1fp, = self.a1.plot([0,0], [-100,100], 'g--', linewidth=2.0)

        toolbar = NavigationToolbar2TkAgg(self.canvas, self.left_frame)
        toolbar.update()
        self.canvas._tkcanvas.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

        # RSSI
        self.middle_frame = tk.Frame(self.center_frame, bg='white', pady=3)
        self.middle_frame.grid(row=0, column=1, sticky="sw")

        self.rssi_fig = Figure(figsize=(2,6), dpi=100)
        self.rssi_a0 = self.rssi_fig.add_subplot(211)
        self.rssi_a1 = self.rssi_fig.add_subplot(212, sharex=self.rssi_a0)
        self.rssi_canvas = FigureCanvasTkAgg(self.rssi_fig, self.middle_frame)
        self.rssi_canvas.show()
        self.rssi_canvas.get_tk_widget().pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True)
        self.rssi_stats = tk.Label(self.middle_frame, text="RSSI", font=("Verdana", 10))
        self.rssi_stats.pack(pady=10,padx=10)
        
        # PDOA
        self.right_frame = tk.Frame(self.center_frame, bg='white', pady=3)
        self.right_frame.grid(row=0, column=2, sticky="sw")

        self.pdoa_fig = Figure(figsize=(4,6), dpi=100)
        self.pdoa_ax = self.pdoa_fig.add_subplot(111)
        self.pdoa_canvas = FigureCanvasTkAgg(self.pdoa_fig, self.right_frame)
        self.pdoa_canvas.show()
        self.pdoa_canvas.get_tk_widget().pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True)
        self.pdoa_stats = tk.Label(self.right_frame, text="stats", font=("Verdana", 10))
        self.pdoa_stats.pack(pady=10,padx=10)
        
        self.resetplot()
        self.updateplot(data_queue)


    def resetplot(self):
        self.history=[]
        
    def updateplot(self, q):
        try:       #Try to check if there is data in the queue
            result=q.get_nowait()
        except:
            self.parent.after(10,self.updateplot,q)
            return

        if result !='Q':
            self.idx += 1
            self.queue_stats['text']="Queue: {:3d} Rate:{:6.2f}".format(q.qsize(), self.idx/(time.time()-self.tic))
            try:
                self.history.append(result)
            except:
                self.history = [result]

            div = 1
            if (q.qsize()>10):
                div = 10
                
            if (self.idx%div==0):
                try:
                    cir = result['cir0']
                    self.line_0r.set_xdata(xrange(len(cir['real'])))
                    self.line_0r.set_ydata([float(x) for x in cir['real']])
                    self.line_0i.set_xdata(xrange(len(cir['imag'])))
                    self.line_0i.set_ydata([float(x) for x in cir['imag']])
                    ymin = np.min([float(x) for x in cir['real']])
                    if (ymin < self.cir_ymin and ymin > -65000):
                        self.cir_ymin = ymin
                        self.a0.set_ylim([self.cir_ymin, self.cir_ymax])
                        self.a1.set_ylim([self.cir_ymin, self.cir_ymax])
                    mag = [np.sqrt(float(x*x)+float(y*y)) for x,y in zip(cir['real'], cir['imag'])]
                    ymax = np.max(mag)
                    if (ymax > self.cir_ymax and ymax < 65000):
                        self.cir_ymax = ymax
                        self.a0.set_ylim([self.cir_ymin, self.cir_ymax])
                        self.a1.set_ylim([self.cir_ymin, self.cir_ymax])
                
                    self.line_0a.set_xdata(xrange(len(cir['real'])))
                    self.line_0a.set_ydata(mag)
                    try:
                        fp_idx = float(cir['fp_idx'])
                        acc_idx = np.floor(fp_idx + 0.5)
                        acc_adj = fp_idx - acc_idx
                        self.line_0fp.set_xdata([8+acc_adj, 8+acc_adj])
                        self.line_0fp.set_ydata([0, 0.9*self.cir_ymax])
                    except:
                        pass

                    self.a0.draw_artist(self.line_0r)
                    self.a0.draw_artist(self.line_0i)
                    self.a0.draw_artist(self.line_0a)
                    self.a0.draw_artist(self.line_0fp)
                    self.canvas.draw()
                except:
                    pass
                try:
                    cir = result['cir1']
                    self.line_1r.set_xdata(xrange(len(cir['real'])))
                    self.line_1r.set_ydata([float(x) for x in cir['real']])
                    self.line_1i.set_xdata(xrange(len(cir['imag'])))
                    self.line_1i.set_ydata([float(x) for x in cir['imag']])
                    self.line_1a.set_xdata(xrange(len(cir['real'])))
                    mag = [np.sqrt(float(x*x)+float(y*y)) for x,y in zip(cir['real'], cir['imag'])]
                    self.line_1a.set_ydata(mag)
                    try:
                        fp_idx = float(cir['fp_idx'])
                        acc_idx = np.floor(fp_idx + 0.5)
                        acc_adj = fp_idx - acc_idx
                        self.line_1fp.set_xdata([8+acc_adj, 8+acc_adj])
                        self.line_1fp.set_ydata([0, 0.9*self.cir_ymax])
                    except:
                        pass
                    
                    self.a1.draw_artist(self.line_1r)
                    self.a1.draw_artist(self.line_1i)
                    self.a1.draw_artist(self.line_1a)
                    self.canvas.draw()
                except:
                    pass

            if (q.qsize()>10):
                if (self.idx%100==0):
                    self.drawHistogram(result, self.pdoa_stats, self.pdoa_ax, self.pdoa_canvas, max_hist=1000)
                if (self.idx%50==0):
                    self.drawHistogram(result, self.rssi_stats, self.rssi_a0, self.rssi_canvas, field='rssi0', max_hist=200)
                    self.drawHistogram(result, self.rssi_stats, self.rssi_a1, self.rssi_canvas, field='rssi1', max_hist=200)
                self.parent.after(0, self.updateplot, q)
            else:
                if (self.idx%50==0):
                    self.drawHistogram(result, self.pdoa_stats, self.pdoa_ax, self.pdoa_canvas, max_hist=1000)
                if (self.idx%10==0):
                    self.drawHistogram(result, self.rssi_stats, self.rssi_a0, self.rssi_canvas, field='rssi0', max_hist=200)
                    self.drawHistogram(result, self.rssi_stats, self.rssi_a1, self.rssi_canvas, field='rssi1', max_hist=200)
                self.parent.after(10, self.updateplot, q)

        else:
            print('done')

    def pdoa_filter(self, data, m=2):
        a=np.array(data)
        a=a[abs(a - np.mean(a)) < m * np.std(a)]
        return a
            
    def drawHistogram(self, d, stats_label, fig_axis, fig_canvas, field='pd', max_hist=None):
        n_bins = 64
        filter_m = 0

        pdata = []
        if max_hist == None:
            h=self.history
        else:
            h=self.history[-max_hist:]
            
        for x in h:
            try:
                if (field=='pd'):
                    pdata.append(float(x[field])*180.0/np.pi)
                else:
                    pdata.append(float(x[field]))
            except:
                pass
        if (filter_m):
            pdata = self.pdoa_filter(pdata, m=filter_m)
        if len(pdata) < 10: return
        
        stats = "Hist({}):{:04d} average: {:.3f} stddev:  {:.3f}".format(field, len(pdata), np.mean(pdata), np.std(pdata))    
        print(stats)
        stats_label['text']=stats
        fig_axis.cla()
        fig_axis.set_xlabel(field)
        fig_axis.hist(pdata, bins='auto', normed=0, rwidth=0.85)
        fig_canvas.draw()
        
        # self.pdoa_ax.title("Pdoa " + stats)

        
class ListenerData:
    def __init__(self, socket_s=None, serial_s=None):
        self.tcp_s = socket_s;
        self.serial_s = serial_s;

    def readlines_socket(self, sock, recv_buffer=4096, delim='\n'):
        buffer = ''
        data = True
        while data:
            data = sock.recv(recv_buffer)
            buffer += data
        
            while buffer.find(delim) != -1:
                line, buffer = buffer.split('\n', 1)
                yield line
        return

    def readlines_serial(self, sock, recv_buffer=4096, delim='\n'):
        buffer = ''
        data = True
        while data:
            data = sock.read(recv_buffer)
            buffer += data
        
            while buffer.find(delim) != -1:
                line, buffer = buffer.split('\n', 1)
                yield line
        return
    
    def readlines(self):
        if self.tcp_s != None:
            return self.readlines_socket(self.tcp_s)
        else:
            return self.readlines_serial(self.serial_s)


   
def input_thread(q, data=None):
    tic = time.time()
    if not data: print("No input data")
    
    for line in data.readlines():
        try:
            d=json.loads(line)
            if (q.qsize()<100):
                q.put(d)
        except:
            continue

        # print(d)
    q.put('Q')

    
if __name__ == '__main__':
    TCP_IP = '127.0.0.1'
    TCP_PORT = 19021
    BUFFER_SIZE = 1024

    parser = argparse.ArgumentParser()
    parser.add_argument('connections', metavar='connection[s]', nargs='+', help='serial device or tcp-port (for rtt)')
    parser.add_argument('-b',metavar='baudrate', type=int,dest='baudrate',default=460800,help='baudrate')

    args = parser.parse_args()
    
    if (len(args.connections)<1):
        print("Defaulting to RTT using port 19021")
        args.conns += "19021"

        #exit(2)

    indata = None
    for conn in args.connections:
        # Check for tcp port
        try:
            c = conn.split(':')
            ip = c[0]
            port = int(c[1])
            print("TCP connection to {:s} port {:d}".format(ip,port))
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((ip, port))
            indata = ListenerData(socket_s=s)
            # run(socket_s=s, doblit=False)
            break;
        except ValueError:
            # Fallback to trying as a serial port
            try:
                s = serial.Serial(conn,baudrate=args.baudrate, timeout=1)
                print('Serial port opened:' + s.name)
                s.flush()
                indata = ListenerData(serial_s=s)
                # run(socket_s=s, doblit=False)
                break;
                # run(serial_s=s, doblit=False)
            except serial.serialutil.SerialException:
                print('Could not open ' + self.s_devname)
                exit(2)

    if (not indata): exit(2)
    app = PdoaApp(data_feed=indata)
    app.mainloop()

    
