#!/usr/bin/env python
# coding=utf-8

import argparse, re, os
import numpy as np
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser()
parser.add_argument('-l', '--logfile', required=True, help='Log file')
parser.add_argument('-m', '--metric', required=True, choices=[ 'latency', 'throughput' ], help='Which metric to plot, "latency" or "throughput"')
parser.add_argument('-s', '--stats', required=True, choices=[ 'avg', 'min', 'max' ], nargs='+', help='Which stat to print, "avg", "min", or "max"')
args = parser.parse_args()

min_bytes = 0
max_rank = 0

entries = {}
with open(args.logfile) as f:
    for line in f:
        # arr = filter(None, line.split(' '))
        # round = int(arr[0])
        # rank = int(arr[1])
        # bytes = int(arr[2])
        # elapsed = float(arr[3])
        # throughput = float(arr[4])

        m = re.compile(r'^\[info\]  round = (\d+), rank = (\d+), bytes recv\'d = (\d+), elapsed = ([\d|\.]+)µsec, throughput = ([\d|\.]+) gbits.$').match(line)
        if not m:
            continue

        round = int(m.group(1))
        rank = int(m.group(2))
        bytes = int(m.group(3))
        elapsed = float(m.group(4))
        throughput = float(m.group(5))

        if min_bytes == 0:
            min_bytes = bytes

        bytes /= min_bytes

        if rank > max_rank:
            max_rank = rank

        if round < 2:
            continue

        if bytes not in entries:
            entries[bytes] = []
        entries[bytes].append([round, rank, elapsed, throughput])

ranks = max_rank + 1
data = {}
stdevs = {}
for k, v in entries.iteritems():
    bytes = k
    arr = v
    t = {}
    for [round, rank, elapsed, throughput] in v:
        # Use round if 1-N and N-1, and (round, rank) for N-N
        if args.metric == 'latency':
            metric = elapsed
        elif args.metric == 'throughput':
            metric = throughput

        if (round, rank) not in t:
            t[(round, rank)] = 0
        t[(round, rank)] += metric

    # print [v for v in t.itervalues()]
    avgv = sum(t.itervalues()) / len(t)
    minv = min(t.itervalues())
    maxv = max(t.itervalues())
    stdev = np.std([v for v in t.itervalues()])
    data[bytes] = (minv, avgv, maxv)
    stdevs[bytes] = stdev
    # print bytes, avgv, minv, maxv, stdev
    # for round, throughput in t.iteritems():
        # print bytes, round, throughput

N = len(data)
np_x = np.array([bytes for bytes, _ in sorted(data.iteritems())])
np_avg = np.array([item[1] for _, item in sorted(data.iteritems())])
np_std = np.array([stdev for _, stdev in sorted(stdevs.iteritems())])
np_min = np.array([item[0] for _, item in sorted(data.iteritems())])
np_max = np.array([item[2] for _, item in sorted(data.iteritems())])

if 'avg' in args.stats:
    plt.errorbar(np_x, np_avg, np_std, label='avg', marker='.', linewidth=0.75)
if 'min' in args.stats:
    plt.plot(np_x, np_min, label='min', marker=".", linewidth=0.75)
if 'max' in args.stats:
    plt.plot(np_x, np_max, label='max', marker=".", linewidth=0.75)

plt.xlabel('Message size per node (bytes)')
plt.xscale('log', basex=2)
plt.xlim(min(np_x), max(np_x))
if args.metric == 'latency':
    plt.ylabel('Latency (usec)')
    #plt.ylim(0, 1000)
else:
    plt.ylabel('Throughput (Gbps)')
    plt.ylim(0, 100)

name = os.path.basename(args.logfile).split('.')[0]
plt.title(name + ' Traffic, 2 + 8 iterations')
plt.legend()

plt.show()

