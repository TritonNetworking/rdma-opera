#!/usr/bin/env python
# coding=utf-8

import argparse, re, os
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument('-l', '--logs', required=True, nargs='+', type=argparse.FileType('r'), help='Log files')
parser.add_argument('--show-legend', required=False, action='store_true', help='Show legend')
parser.add_argument('-e', '--export', required=False, type=argparse.FileType('w'), help='Export to CSV file')
parser.add_argument('-p', '--plot', required=True, choices=[ 'cdf', 'time' ], help='The type of plot')
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
warmup = 30     # warmup time

def process_file(f):
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

    rtts = rtts[(warmup * RATE):]   # Discard the warmup data
    if args.plot == 'cdf':
        x = np.sort(rtts)
        y = np.arange(len(x)) / float(len(x))
    else:
        x = np.arange(len(rtts))
        y = np.array(rtts)
    plt.plot(x, y, linewidth=0.75, label=name)

    if args.stats:
        rtt_sorted = np.sort(rtts)
        if len(rtt_sorted) > 0:
            percent90 = rtt_sorted[int(len(rtt_sorted) * 0.90)]
            percent99 = rtt_sorted[int(len(rtt_sorted) * 0.99)]
            minv = min(rtt_sorted)
            maxv = max(rtt_sorted)
            average = sum(rtt_sorted) / len(rtt_sorted)
            median = rtt_sorted[len(rtt_sorted - 1) / 2]
            print "count = %d, min = %f, max = %f, average = %f, median = %f, 90 = %f, 99 = %f" \
                    % (len(rtt_sorted), minv, maxv, average, median, percent90, percent99)
        else:
            print "count = %d, no data in file" % len(rtt_sorted)
    return rtts if args.export else []

def main():
    plt.figure(figsize=(8, 6))
    if args.export:
        mat = []
    for f in args.logs:
        print 'Processing "%s" ...' % f.name
        rtts = process_file(f)
        if args.export:
            mat.append(rtts)
    if args.plot == 'cdf':
        plt.xlabel('RTT (us)')
        plt.xlim(0.0, 40.0)
    else:
        plt.xlabel('sequence #')
    #plt.ylim(0.0, 1.0)
    if args.show_legend:
        plt.legend()
    if args.output:
        print 'Saving figure to "%s" ...' % args.output.name
        plt.savefig(args.output.name, dpi=300)
    else:
        print 'Showing plot ...'
        plt.show()

    if args.export:
        length = len(mat[0])
        for index in xrange(length):
            arr = [mat[i][index] for i in xrange(len(mat))]
            line = ",".join([str(x) for x in arr]) + "\n"
            args.export.write(line)

if __name__ == "__main__":
    main()

