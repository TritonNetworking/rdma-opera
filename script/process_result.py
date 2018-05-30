#!/usr/bin/env python

import argparse, re, scanf
import numpy as np
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser()
parser.add_argument('-l', '--log', required=True, help='Log source: "mine" or "mellanox". Whether the logs are from my tool or Mellanox tool.')
parser.add_argument('-m', '--mode', required=True, help='Measurement mode: "latency" or "throughput"')
parser.add_argument('-c', '--client-log', required=True, help='Client log file')
parser.add_argument('-s', '--server-log', required=True,help='Server log file')
args = parser.parse_args()

if args.log != 'mine' and args.log != 'mellanox':
    raise ValueError('Mode must be either "mine" or "mellanox"')

if args.mode != 'latency' and args.mode != 'throughput':
    raise ValueError('Mode must be either "latency" or "throughput"')

latencies = {}
throughputs = {}
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

if args.log == 'mine':
    block_size = 0
    latency_header = False
    with open(args.client_log) as f:
        for line in f:
            line = strip_tag(line)
            m = re.compile(r'^Length = (\d+)').match(line)
            if m:
                block_size = int(m.group(1))
                continue

            if args.mode == 'latency':
                if '#bytes, #iterations, median, average, min, max, stdev, percent90, percent99' in line:
                    latency_header = True
                    continue
                if latency_header:
                    latency = transform_latency(line)
                    if int(latency[0]) != block_size:
                        raise ValueError('Inconsistent length in latency report')
                    latencies[block_size] = latency
                    latency_header = False
            else:
                m = re.compile(r'Transferred: (\d+) B, elapsed: (\d+\.\d+)e([+-]\d+) s, throughput: (\d+\.\d+) Gbps.').match(line)
                if m:
                    (total_length, elapsed_coeff, elapsed_exp, throughput) = m.groups()
                    total_length = int(total_length)
                    elapsed = float(elapsed_coeff) * pow(10, int(elapsed_exp))
                    throughput = float(throughput)
                    throughputs[block_size] = throughput

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

    if args.mode == 'latency':
        N = len(latencies)
        x = np.array([block_size for block_size, _ in sorted(latencies.iteritems())])
        medians = np.array([latencies[block_size][3] for block_size, _ in sorted(latencies.iteritems())])
        stdevs = np.array([latencies[block_size][6] for block_size, _ in sorted(latencies.iteritems())])
        plt.errorbar(x, medians, stdevs, marker='')
        plt.xscale('log')
        plt.show()
    else:
        x = np.array([block_size for block_size, _ in sorted(throughputs.iteritems())])
        y = np.array([throughput for _, throughput in sorted(throughputs.iteritems())])
        plt.plot(x, y, marker='o')
        plt.xscale('log')
        plt.show()
else:
    header = False
    with open(args.client_log) as f:
        for line in f:
            line = line.strip()
            if line.startswith('#bytes'):
                header = True
                continue
            if not header:
                continue
            if args.mode == 'latency':
                if not re.compile(r'^\d+').match(line):
                    break
                arr = [float(x) for x in filter(None, line.split(' '))]
                block_size = int(arr[0])
                latencies[block_size] = arr
            else:
                if not re.compile(r'^\d+').match(line):
                    break
                arr = [float(x) for x in filter(None, line.split(' '))]
                block_size = int(arr[0])
                throughputs[block_size] = arr[3]

    if args.mode == 'latency':
        x = np.array([block_size for block_size, _ in sorted(latencies.iteritems())])
        averages = np.array([latencies[block_size][5] for block_size, _ in sorted(latencies.iteritems())])
        stdevs = np.array([latencies[block_size][6] for block_size, _ in sorted(latencies.iteritems())])
        plt.errorbar(x, averages, stdevs, marker='')
        plt.xscale('log')
        plt.show()
    else:
        x = np.array([block_size for block_size, _ in sorted(throughputs.iteritems())])
        y = np.array([throughput for _, throughput in sorted(throughputs.iteritems())])
        plt.plot(x, y, marker='o')
        plt.xscale('log')
        plt.show()
