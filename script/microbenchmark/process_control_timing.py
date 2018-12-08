#!/usr/bin/env python
# coding=utf-8

import argparse, re, os, sys
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument('-l', '--logs', required=True, nargs='+', type=argparse.FileType('r'), help='Log files')
parser.add_argument('--show-legend', required=False, action='store_true', help='Show legend')
parser.add_argument('-p', '--plot', required=True, choices=[ 'cdf', 'time', 'scatter' ], help='The type of plot')
parser.add_argument('-o', '--output', type=argparse.FileType('w'), help='Output plot to file')
parser.add_argument('-s', '--stats', action='store_true', help='Whether to show stats')
args = parser.parse_args()

if args.output:
    # Workaround w/o X window server
    import matplotlib
    matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.ticker import AutoMinorLocator

# Plot options
LINEWIDTH=0.75

HEADER="[verbose] Start,End,Latency"
FOOTER="[verbose]"
regex_latency = re.compile('\[verbose\] ([\d.]+),([\d.]+),([\d.]+)')
GROUPSIZE=8

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
    plt.scatter(x, y, s=0.5, label=name)

def set_plot_options():
    ax = plt.gca()
    minor_locator = AutoMinorLocator(5)
    ax.yaxis.set_minor_locator(minor_locator)
    ax.grid(which='minor', alpha=0.2)
    ax.grid(which='major', alpha=0.5)
    if args.plot == 'cdf':
        plt.xlabel('RTT (us)')
        #plt.xlim(0.0, 40.0)
        plt.ylabel('CDF')
        #plt.ylim(0.0, 1.0)
        plt.title('First send to last ACK (8 sequential messages)')
    else:
        plt.xlabel('Sequence #')
        #plt.ylim(0.0, 1000.0)
        plt.ylabel('Time (us)')
        plt.title('First send to last ACK (8 sequential messages)')
    if args.show_legend:
        plt.legend()

def print_stats(l):
    print 'count,min,max,average,median,percent90,percent99'
    if len(l) == 0:
        print >> sys.stderr, "no data in file"
        return
    s = np.sort(l)
    p90 = s[int(len(s) * 0.90)]
    p99 = s[int(len(s) * 0.99)]
    minv = min(s)
    maxv = max(s)
    avg = sum(s) / len(s)
    med = s[len(s - 1) / 2]
    print "%d,%f,%f,%f,%f,%f,%f" % (len(s), minv, maxv, avg, med, p90, p99)

def get_shortname(filename):
    return os.path.split(filename)[-1].split('.')[0]

# Remove leading tag like '[debug] '
def strip_tag(line, name):
    if name is None:
        m = re.compile(r'^\[.*\] *').match(line)
    else:
        m = re.compile('^\[%s\] *' % name).match(line)
    tag = m.group() if m else ''
    return line[len(tag):]

def process_log(f):
    rtts = []
    group = []
    count = 0
    valid = False
    first_start = None
    last_end = None
    for line in f:
        line = line.rstrip()
        if line == HEADER:
            valid=True
            continue
        if not valid:
            continue
        if line == FOOTER:
            break
        m = regex_latency.match(line)
        if not m:
            print >> sys.stderr, "Unrecognized line: %s" % line
        start = float(m.group(1))
        end = float(m.group(2))
        rtt = float(m.group(3))
        group.append(rtt)
        if count % GROUPSIZE == 0:
            first_start = start
        if count % GROUPSIZE == GROUPSIZE - 1:
            last_end = end
        count += 1
        if count % GROUPSIZE == 0:
            group_latency = last_end - first_start
            rtts.append(group_latency)
            #rtts.extend(group)
            group = []
    #end
    return (rtts, group)

def plot_log(rtts, name):
    if args.plot == 'cdf':
        plot_cdf(rtts, name)
    elif args.plot == 'time':
        plot_time(rtts, name)
    elif args.plot == 'scatter':
        plot_scatter(rtts, name)
    #end

def main():
    plt.figure(figsize=(8, 6))
    for index in xrange(len(args.logs)):
        f = args.logs[index]
        print >> sys.stderr, 'Processing "%s" ...' % f.name
        name = get_shortname(f.name)
        (rtts, group) = process_log(f)
        if len(group) != 0:
            print >> sys.stderr, "Unexpected leftover RTTs:", group
        plot_log(rtts, name)
        if args.stats:
            print_stats(rtts)
    #end
    set_plot_options()
    if args.output:
        print >> sys.stderr, 'Saving figure to "%s" ...' % args.output.name
        plt.savefig(args.output.name, dpi=300)
    else:
        print >> sys.stderr, 'Showing plot ...'
        plt.show()

if __name__ == "__main__":
    main()

