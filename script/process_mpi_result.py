#!/usr/bin/env python
# coding=utf-8

import argparse, re, os
import numpy as np
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser()
parser.add_argument('-l', '--logfile', required=True, help='Log file')
args = parser.parse_args()

min_bytes = 0

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

        if round < 2:
            continue

        if bytes not in entries:
            entries[bytes] = []
        entries[bytes].append([round, rank, elapsed, throughput])

avgs = {}
stdevs = {}
for k, v in entries.iteritems():
    bytes = k
    arr = v
    t = {}
    for [round, rank, elapsed, throughput] in v:
        if round not in t:
            t[round] = 0
        t[round] += throughput
    avg = sum(t.itervalues()) / len(t)
    # print [v for v in t.itervalues()]
    stdev = np.std([v for v in t.itervalues()])
    avgs[bytes] = avg
    stdevs[bytes] = stdev
    # print bytes, avg, stdev
    # for round, throughput in t.iteritems():
        # print bytes, round, throughput

N = len(avgs)
np_x = np.array([bytes for bytes, _ in sorted(avgs.iteritems())])
np_avg = np.array([avg for _, avg in sorted(avgs.iteritems())])
np_std = np.array([stdev for _, stdev in sorted(stdevs.iteritems())])
plt.errorbar(np_x, np_avg, np_std, marker='.')

plt.xscale('log', basex=2)
plt.ylim(0, 100)

plt.xlabel('Message size per node (bytes)')
plt.ylabel('Throughput (Gbps)')

name = os.path.basename(args.logfile).split('.')[0]
plt.title(name + ' Traffic, 2 + 8 iterations')

plt.show()

