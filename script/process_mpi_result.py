#!/usr/bin/env python
# coding=utf-8

import argparse, re, os
import numpy as np
import matplotlib.pyplot as plt

# Note: Make sure this is the same as set in the program
ROUNDS = 10

parser = argparse.ArgumentParser()
parser.add_argument('--mpi-logs', required=False, nargs='+', type=argparse.FileType('r'), help='MPI Log file')
parser.add_argument('--rdma-logs', required=False, nargs='+', type=argparse.FileType('r'), help='Mellanox RDMA Log file')
parser.add_argument('-m', '--metric', required=True, choices=[ 'latency', 'throughput' ], help='Which metric to plot, "latency" or "throughput"')
parser.add_argument('-s', '--stats', required=True, choices=[ 'avg', 'errorbar', 'min', 'max' ], nargs='+', help='Which stat to print, "avg", "errorbar", "min", or "max"')
args = parser.parse_args()

def plot_mpi_logfile(logfile):
    min_bytes = 0
    max_rank = 0

    entries = {}
    #with open(logfile) as f:
    with logfile as f:
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

            if round < 2 or round >= ROUNDS - 1:
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
                metric = elapsed / min_bytes
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

    name = os.path.basename(logfile.name).split('.')[0]
    if 'errorbar' in args.stats:
        plt.errorbar(np_x, np_avg, np_std, label='%s - errorbar' % name, marker='.', linewidth=0.75)
    if 'avg' in args.stats:
        plt.plot(np_x, np_avg, label='%s - avg' % name, marker='.', linewidth=0.75)
    if 'min' in args.stats:
        plt.plot(np_x, np_min, label='%s - min' % name, marker=".", linewidth=0.75)
    if 'max' in args.stats:
        plt.plot(np_x, np_max, label='%s - max' % name, marker=".", linewidth=0.75)

    return (min(np_x), max(np_x))

def plot_rdma_logfile(logfile):
    data = {}
    stdevs = {}
    header = False
    with logfile as f:
        for line in f:
            if args.metric == 'latency':
                if 't_typical' in line:
                    header = True
                    continue
                if header:
                    if '-------' in line:
                        break

                    #print line
                    #print [x for x in filter(None, line.strip().split(' '))]
                    arr = [float(x) for x in filter(None, line.strip().split(' '))]
                    bytes = int(arr[0])
                    avgv = arr[5]
                    minv = arr[2]
                    maxv = arr[3]
                    stdev = arr[6]
                    data[bytes] = (minv, avgv, maxv)
                    stdevs[bytes] = stdev
            else:
                if 'BW average[Gb/sec]' in line:
                    header = True
                    continue
                if header:
                    if '-------' in line:
                        break

                    arr = [float(x) for x in filter(None, line.strip().split(' '))]
                    bytes = int(arr[0])
                    avgv = arr[3]
                    minv = 0
                    maxv = 0
                    stdev = 0
                    data[bytes] = (minv, avgv, maxv)
                    stdevs[bytes] = stdev

    N = len(data)
    np_x = np.array([bytes for bytes, _ in sorted(data.iteritems())])
    np_avg = np.array([item[1] for _, item in sorted(data.iteritems())])
    np_std = np.array([stdev for _, stdev in sorted(stdevs.iteritems())])
    np_min = np.array([item[0] for _, item in sorted(data.iteritems())])
    np_max = np.array([item[2] for _, item in sorted(data.iteritems())])

    name = os.path.basename(logfile.name).split('.')[0]
    if 'errorbar' in args.stats:
        plt.errorbar(np_x, np_avg, np_std, label='%s - errorbar' % name, marker='.', linewidth=0.75)
    if 'avg' in args.stats:
        plt.plot(np_x, np_avg, label='%s - avg' % name, marker='.', linewidth=0.75)
    if 'min' in args.stats:
        plt.plot(np_x, np_min, label='%s - min' % name, marker=".", linewidth=0.75)
    if 'max' in args.stats:
        plt.plot(np_x, np_max, label='%s - max' % name, marker=".", linewidth=0.75)

    return (min(np_x), max(np_x))

if args.mpi_logs:
    for logfile in args.mpi_logs:
        plot_mpi_logfile(logfile)
if args.rdma_logs:
    for logfile in args.rdma_logs:
        plot_rdma_logfile(logfile)

plt.xlabel('Message size per node (bytes)')
plt.xscale('log', basex=2)
#if args.metric == 'latency':
#    plt.yscale('log', basey=10)

#plt.xlim(min(np_x), max(np_x))
plt.xlim(1, pow(2, 18))
if args.metric == 'latency':
    plt.ylabel('Latency (usec)')
    plt.ylim(0, 40)
else:
    plt.ylabel('Throughput (Gbps)')
    plt.ylim(0, 100)

#if len(args.mpi_logs) + len(args.rdma_logs) == 1:
    #name = os.path.basename(args.logfiles[0].name).split('.')[0]
    #plt.title(name + ' Traffic, 2 + 8 iterations')
#    0
#else:
#    plt.title('MPI Traffic, 2 + 8 iterations')
if not args.rdma_logs:
    plt.title('MPI Traffic, 2 + 8 iterations')
elif not args.mpi_logs:
    plt.title('RDMA Traffic, 2 + 8 iterations')
else:
    plt.title('MPI/RDMA Traffic, 2 + 8 iterations')

plt.legend()

plt.show()

