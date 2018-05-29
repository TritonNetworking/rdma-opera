#!/usr/bin/env python

import argparse, re, scanf
import numpy as np
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser()
parser.add_argument('-m', '--mode', help='Measurement mode: "latency" or "throughput"')
parser.add_argument('-c', '--client-log', required=True, help='Client log file')
parser.add_argument('-s', '--server-log', required=True,help='Server log file')
args = parser.parse_args()

if args.mode and args.mode != 'latency' and args.mode != 'throughput':
    raise ValueError('Mode must be either latency or throughput')

data = {}
checksum = {}

# Remove leading tag like '[debug] '
def strip_tag(line):
    m = re.compile(r'^\[.*\] *').match(line)
    tag = m.group() if m else ''
    return line[len(tag):]

def transform_latency(line):
    vals = []
    for e in line.split(','):
        vals.append(float(e))
    return vals

block_size = 0
latency_header = False
with open(args.client_log) as f:
    for line in f:
        line = strip_tag(line)
        m = re.compile(r'^Length = (\d+)').match(line)
        if m:
            block_size = int(m.group(1))
            continue
        if '#bytes, #iterations, median, average, min, max, stdev, percent90, percent99' in line:
            latency_header = True
            continue
        if latency_header:
            latency = transform_latency(line)
            if int(latency[0]) != block_size:
                raise ValueError('Inconsistent length in latency report')
            data[block_size] = latency
            latency_header = False
        m = re.compile(r'^SHA1 sum: count = (\d+), length = (\d+), digest = (\w+).$').match(line)
        if m:
            (count, length, digest) = m.groups()
            length = int(length)
            checksum[length] = digest

block_size = 0
with open(args.server_log) as f:
    for line in f:
        m = re.compile(r'^Length = (\d+)').match(line)
        if m:
            block_size = int(m.group(1))
            continue
        m = re.compile(r'^SHA1 sum: count = (\d+), length = (\d+), digest = (\w+).$').match(line)
        if m:
            (count, length, digest) = m.groups()
            length = int(length)
            if checksum[length] != digest:
                print 'Incorrect checksum for length %d' % length

N = len(data)
x = np.array([block_size for block_size, _ in sorted(data.iteritems())])
medians = np.array([data[block_size][3] for block_size, _ in sorted(data.iteritems())])
stdevs = np.array([data[block_size][6] for block_size, _ in sorted(data.iteritems())])

plt.errorbar(x, medians, stdevs)
plt.xscale('log')
plt.show()
