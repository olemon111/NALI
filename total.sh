#!/bin/bash

# evaluation
# ./run.sh /home/zzy/dataset/generate_random_osm_longlat.dat binary integer 10000000 110000000 insert 0 alex 1 0 > alex-longitudes-search.data
# ./run.sh longitudes-200M.bin.data binary double 100000000 200000000 insert 1 alex 1 0 > alex-longitudes-insert.data

./build.sh
# sudo rm -rf /mnt/pmem0/zzy/*
# sudo rm -rf /mnt/pmem1/zzy/*
# sudo mkdir -p /mnt/pmem0/zzy
# sudo mkdir -p /mnt/pmem1/zzy
cd script
sudo ./multi_bench.sh
cd ..