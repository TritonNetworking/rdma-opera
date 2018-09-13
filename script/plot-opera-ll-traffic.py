#!/usr/bin/env python
# coding=utf-8

import argparse, re, os
import numpy as np
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser()
parser.add_argument('-l', '--logs', required=True, nargs='+', type=argparse.FileType('r'), help='Log files')
parser.add_argument('-o', '--output', required=False, type=argparse.FileType('w'), help='Combined output CSV file')
parser.add_argument('-p', '--plot', required=True, choices=[ 'cdf', 'time' ], help='The type of plot')
parser.add_argument('-s', '--stats', action="store_true", help='Whether to show stats')
args = parser.parse_args()

GAP = 0
#GAP = 1000

def plot_cdf(f):
    name = os.path.split(f.name)[-1].split('.')[0]
    header=True
    rtts = []
    for line in f:
        line = line.rstrip()
        if line == "#, usec":
            header=False
            continue
        if header:
            continue
        if line.startswith(" "):
            break
        splitted = line.split(", ")
        index = int(splitted[0])
        latency = float(splitted[1])
        rtt = 2 * (latency - GAP)
        rtts.append(rtt)
    #print len(rtt)
    x = np.sort(rtts)
    y = np.arange(len(x)) / float(len(x))
    plt.plot(x, y, linewidth=0.75, label=name)
    if args.stats:
        percent90 = x[int(len(x) * 0.90)]
        percent99 = x[int(len(x) * 0.99)]
        minv = min(x)
        maxv = max(x)
        print "min = %f, max = %f, 90 = %f, 99 = %f" % (minv, maxv, percent90, percent99)
    return rtts if args.output else []

def plot_time_series(f):
    name = os.path.split(f.name)[-1].split('.')[0]
    header=True
    rtts = []
    for line in f:
        line = line.rstrip()
        if line == "#, usec":
            header=False
            continue
        if header:
            continue
        if line.startswith(" "):
            break
        splitted = line.split(", ")
        index = int(splitted[0])
        latency = float(splitted[1])
        rtt = 2 * (latency - GAP)
        rtts.append(rtt)
    #print len(rtt)
    x = np.arange(len(rtts))
    y = np.array(rtts)
    plt.plot(x, y, linewidth=0.75, label=name)
    if args.stats:
        rtt_sorted = np.sort(rtts)
        percent90 = rtt_sorted[int(len(rtt_sorted) * 0.90)]
        percent99 = rtt_sorted[int(len(rtt_sorted) * 0.99)]
        minv = min(rtt_sorted)
        maxv = max(rtt_sorted)
        print "min = %f, max = %f, 90 = %f, 99 = %f" % (minv, maxv, percent90, percent99)
    return rtts if args.output else []

def main():
    mat = []
    for f in args.logs:
        if args.plot == 'cdf':
            rtts = plot_cdf(f)
        else:
            rtts = plot_time_series(f)
        mat.append(rtts)
    plt.xlim(0.0)
    #plt.ylim(0.0, 1.0)
    plt.legend()
    plt.show()
    if args.output:
        length = len(mat[0])
        for index in xrange(length):
            arr = [mat[i][index] for i in xrange(len(mat))]
            line = ",".join([str(x) for x in arr]) + "\n"
            args.output.write(line)

if __name__ == "__main__":
    main()

