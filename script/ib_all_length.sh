#!/usr/bin/env bash

# Mellanox perftest test suite
ib_read_lat -a -R -F $1
ib_read_bw -a -R -F --report_gbits $1
ib_write_lat -a -R -F $1
ib_write_bw -a -R -F --report_gbits $1

