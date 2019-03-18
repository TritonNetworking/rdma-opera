#!/usr/bin/env python
# coding=utf-8

import argparse, re, os, sys
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument('-l', '--logs', required=True, nargs='+', type=argparse.FileType('r'), help='Log files')
parser.add_argument('-t', '--ttls', required=False, nargs='+', type=argparse.FileType('r'), help='TTL files')
parser.add_argument('--show-legend', required=False, action='store_true', help='Show legend')
parser.add_argument('-e', '--export', required=False, type=argparse.FileType('w'), help='Export to CSV file')
parser.add_argument('-p', '--plot', required=True, choices=[ 'cdf', 'time', 'scatter' ], help='The type of plot')
parser.add_argument('--combined', action="store_true", help='Whether to plot combined CDF')
parser.add_argument('-o', '--output', type=argparse.FileType('w'), help='Output plot to file')
parser.add_argument('-s', '--stats', action='store_true', help='Whether to show stats')
args = parser.parse_args()

if args.output:
    # Workaround w/o X window server
    import matplotlib
    matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.ticker import AutoMinorLocator

# LL traffic configuration parameters
GAP = 1000                  # GAP in µs
warmup = 10                 # warmup time in seconds
cooldown = sys.maxint       # cooldown in seconds

# Derived parameters
RATE = int(1e6 / GAP / 2)   # messages per second, both sides have a gap

# Plot options
LINEWIDTH=0.75

def plot_cdf(l, name):
    x = np.sort(l)
    y = np.arange(len(l)) / float(len(l))
    plt.plot(x, y, linewidth=LINEWIDTH, label=name)

def plot_time(l, name):
    x = np.arange(len(l))
    y = np.array(l)
    plt.plot(x, y, linewidth=LINEWIDTH, label=name)

def plot_scatter(l, name):
    x = np.arange(len(l))
    y = np.array(l)
    plt.scatter(x, y, s=1, label=name)

def set_plot_options():
    ax = plt.gca()
    minor_locator = AutoMinorLocator(5)
    ax.yaxis.set_minor_locator(minor_locator)
    ax.grid(which='minor', alpha=0.2)
    ax.grid(which='major', alpha=0.5)
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

def print_stats(l):
    if len(l) == 0:
        print >> sys.stderr, "count = %d, no data in file" % len(l)
        return
    s = np.sort(l)
    p90 = s[int(len(s) * 0.90)]
    p99 = s[int(len(s) * 0.99)]
    p999 = s[int(len(s) * 0.999)]
    p9999 = s[int(len(s) * 0.9999)]
    minv = min(s)
    maxv = max(s)
    avg = sum(s) / len(s)
    med = s[len(s - 1) / 2]
    over1s = sum(1 for e in s if e > 1e6)
    print "%d,%f,%f,%f,%f,%f,%f,%f,%f,%d" \
        % (len(s), minv, maxv, avg, med, p90, p99, p999, p9999, over1s)

def get_shortname(filename):
    return os.path.split(filename)[-1].split('.')[0]

def process_log(f):
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
        # Latency no longer includes the gap
        rtt = 2 * latency
        rtts.append(rtt)
    return rtts

def process_ttl(f):
    ttls = []
    header_count = 1
    for index, line in enumerate(f):
        if index < header_count:
            continue
        line = line.rstrip()
        splitted = line.split(',')
        src = splitted[0]
        dst = splitted[1]
        ttl = int(splitted[2])
        ttls.append(ttl)
    return ttls

def plot_log(rtts, name):
    if args.plot == 'cdf':
        # Discard the warmup data in CDF
        plot_cdf(rtts[int(warmup * RATE):int(cooldown * RATE)], name)
    elif args.plot == 'time':
        plot_time(rtts, name)
    elif args.plot == 'scatter':
        plot_scatter(rtts, name)
    #end
    if args.stats:
        if args.plot == 'cdf' and not args.combined:
            print_stats(rtts[int(warmup * RATE):int(cooldown * RATE)])
        else:
            print_stats(rtts)
    return rtts

def plot_ttl_cdf(arr_rtt, arr_ttl, name):
    d = {}
    rtts = arr_rtt[int(warmup * RATE):]
    ttls = arr_ttl[int(warmup * RATE):]
    for i, rtt in enumerate(rtts):
        ttl = ttls[i]
        if ttl not in d:
            d[ttl] = []
        d[ttl].append(rtt)
    for ttl in d:
        plot_cdf(d[ttl], get_shortname(name) + ', TTL = %d' % ttl)

def main():
    plt.figure(figsize=(8, 6))
    if args.stats or args.export:
        mat = []
    if args.stats:
        print 'count,min,max,average,median,percent90,percent99,percent99.9,percent99.99,over1s'
    assert args.ttls is None or len(args.logs) == len(args.ttls), "Not the same number of TTL files as log files."
    assert args.ttls is None or args.plot == 'cdf', "Only \"cdf\" mode is supported with TTL logs."
    if args.plot == 'cdf' and args.combined:
        all_rtts = []
    for index in xrange(len(args.logs)):
        f = args.logs[index]
        print >> sys.stderr, 'Processing "%s" ...' % f.name
        name = get_shortname(f.name)
        rtts = process_log(f)
        if args.ttls is not None:
            ttls = process_ttl(args.ttls[index])
            assert len(rtts) + 1 == len(ttls), 'Not the same number of TTL entries as RTTs'
            plot_ttl_cdf(rtts, ttls, name)
        elif not (args.plot == 'cdf' and args.combined):
            plot_log(rtts, name)
        if args.plot == 'cdf' and args.combined:
            all_rtts += rtts[int(warmup * RATE):int(cooldown * RATE)]
        if args.stats or args.export:
            mat.append(rtts[int(warmup * RATE):int(cooldown * RATE)])
    #end
    if args.plot == 'cdf' and args.combined:
        plot_log(all_rtts, "all")
    set_plot_options()
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

