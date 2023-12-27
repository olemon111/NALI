#!/bin/bash

# generate data
# sudo perf record -e cpu-clock --call-graph dwarf -p `ps -C multi_bench -o pid=`
# draw flamegraph
sudo perf script -i perf.data | ./stackcollapse-perf.pl | ./flamegraph.pl > ./nap-nali.svg