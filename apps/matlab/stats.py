#!/usr/bin/env python

import socket
import json
import struct
import numpy as np
import time
import matplotlib
matplotlib.use('GTKAgg')
from matplotlib import pyplot as plt


def readlines(sock, recv_buffer=4096, delim='\n'):
    buffer = ''
    data = True
    while data:
	data = sock.recv(recv_buffer)
	buffer += data
        
	while buffer.find(delim) != -1:
	    line, buffer = buffer.split('\n', 1)
	    yield line
    return


def run(s, niter=1000, doblit=True):
    """
    Display the simulation using matplotlib, optionally using blit for speed
    """

    fig, ax = plt.subplots(1, 1)
    ax.set_xlim(0, 255)
    ax.set_ylim(-1, 5)
    ax.hold(True)

    plt.show(False)
    plt.draw()

    if doblit:
        # cache the background
        fig.canvas.draw()
        background = fig.canvas.copy_from_bbox(ax.bbox)
    x=[0]
    y=[0]
    points = ax.plot(x, y, 'k')[0]
    tic = time.time()

    i = 0
    x=[]
    y=[]
    for line in readlines(s):
        try:
            d=json.loads(line)
            d['rangef'] = struct.unpack('!f', struct.pack('!I', d['range']))[0]
            d['rssif'] = struct.unpack('!f', struct.pack('!I', d['rssi']))[0]
            d['toff'] = struct.unpack('!f', struct.pack('!I', d['tof']))[0]
        except:
            continue

        if (len(x) < 255):
            x.append(i)
        else:
            y.pop(0)
        i+=1
        y.append(d['rangef'])
        points.set_data(x,y)

        if doblit:
            # restore background
            fig.canvas.restore_region(background)

            # redraw just the points
            ax.draw_artist(points)

            # fill in the axes rectangle
            fig.canvas.blit(ax.bbox)

        else:
            # redraw everything
            fig.canvas.draw()
        plt.pause(0.0001)

    plt.close(fig)
    print "Blit = %s, average FPS: %.2f" % (
        str(doblit), niter / (time.time() - tic))

if __name__ == '__main__':
    TCP_IP = '127.0.0.1'
    TCP_PORT = 19021
    BUFFER_SIZE = 1024
    MESSAGE = "Hello, World!"
    
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((TCP_IP, TCP_PORT))
    
    run(s, doblit=False)

    
