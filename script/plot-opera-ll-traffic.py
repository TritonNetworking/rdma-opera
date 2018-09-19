#!/usr/bin/env python
# coding=utf-8

import argparse, re, os, sys
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument('-l', '--logs', required=True, nargs='+', type=argparse.FileType('r'), help='Log files')
parser.add_argument('--show-legend', required=False, action='store_true', help='Show legend')
parser.add_argument('-e', '--export', required=False, type=argparse.FileType('w'), help='Export to CSV file')
parser.add_argument('-p', '--plot', required=True, choices=[ 'cdf', 'time', 'scatter' ], help='The type of plot')
parser.add_argument('-o', '--output', type=argparse.FileType('w'), help='Output plot to file')
parser.add_argument('-s', '--stats', action='store_true', help='Whether to show stats')
args = parser.parse_args()

if args.output:
    # Workaround w/o X window server
    import matplotlib
    matplotlib.use('Agg')
import matplotlib.pyplot as plt

GAP = 0
#GAP = 1000
RATE = 1000     # messages per second
warmup = 10 / 2 # warmup time in seconds / 2

def print_stats(l):
    if len(l) == 0:
        print >> sys.stderr, "count = %d, no data in file" % len(s)
        return
    s = np.sort(l)
    p90 = s[int(len(s) * 0.90)]
    p99 = s[int(len(s) * 0.99)]
    minv = min(s)
    maxv = max(s)
    avg = sum(s) / len(s)
    med = s[len(s - 1) / 2]
    over1s = sum(1 for e in s if e > 1e6)
    print "%d,%f,%f,%f,%f,%f,%f,%d" \
        % (len(s), minv, maxv, avg, med, p90, p99, over1s)

def process_log(f):
    name = os.path.split(f.name)[-1].split('.')[0]
    header=True
    rtts = []
    for line in f:
        line = line.rstrip()
        if header and line == "#, usec":
            header=False
            continue
        if header:
            continue
        if not header and line.startswith(" "):
            break
        splitted = line.split(", ")
        if len(splitted) < 2:
            print 'Skipping unexpected line "%s" ...' % line
            continue
        index = int(splitted[0])
        latency = float(splitted[1])
        rtt = 2 * (latency - GAP)
        rtts.append(rtt)
    return rtts

def plot_log(f):
    rtts = process_log(f)
    if args.plot == 'cdf':
        rtts = rtts[int(warmup * RATE):]   # Discard the warmup data
        x = np.sort(rtts)
        y = np.arange(len(x)) / float(len(x))
        plt.plot(x, y, linewidth=0.75, label=name)
    else:
        x = np.arange(len(rtts))
        y = np.array(rtts)
        if args.plot == 'time':
            plt.plot(x, y, linewidth=0.75, label=name)
        if args.plot == 'scatter':
            plt.scatter(x, y, label=name, s=1)
    if args.stats:
        print_stats(rtts)
    return rtts

def main():
    plt.figure(figsize=(8, 6))
    if args.stats or args.export:
        mat = []
    if args.stats:
        print 'count,min,max,average,median,percent90,percent99,over1s'
    for f in args.logs:
        print >> sys.stderr, 'Processing "%s" ...' % f.name
        rtts = plot_log(f)
        if args.stats or args.export:
            mat.append(rtts)
    if args.plot == 'cdf':
        plt.xlabel('RTT (us)')
        plt.xlim(0.0, 40.0)
        #plt.ylim(0.0, 1.0)
    else:
        plt.xlabel('sequence #')
        #plt.ylim(0.0, 1000.0)
        plt.ylabel('RTT (us)')
        #plt.yscale('log')
    if args.show_legend:
        plt.legend()

    if args.output:
        print >> sys.stderr, 'Saving figure to "%s" ...' % args.output.name
        plt.savefig(args.output.name, dpi=300)
    else:
        print >> sys.stderr, 'Showing plot ...'
        plt.show()

    if args.stats:
        print >> sys.stderr, 'Getting stats across all logs ...'
        rtts = reduce(lambda x, y: x + y, mat)
        print_stats(rtts)

    if args.export:
        print >> sys.stderr, 'Exporting all data to CSV ...'
        length = len(mat[0])
        for index in xrange(length):
            arr = [mat[i][index] for i in xrange(len(mat))]
            line = ",".join([str(x) for x in arr]) + "\n"
            args.export.write(line)

if __name__ == "__main__":
    main()

