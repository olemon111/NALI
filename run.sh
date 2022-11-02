#!/bin/bash

thread_num=(1 1)

for j in 1
do
    # rm -f /mnt/pmem1/zzy/template.data
    LD_PRELOAD="./build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1" numactl --cpunodebind=0 --membind=0 ./build/benchmark \
        --keys_file=$1 \
        --keys_file_type=$2 \
        --keys_type=$3 \
        --init_num_keys=100000000 \
        --workload_keys=$4 \
        --total_num_keys=$5 \
        --operation=$6 \
        --lookup_distribution=uniform \
        --theta=0.99 \
        --using_epoch=$7 \
        --thread_num=${thread_num[$j]} \
        --index=$8 \
        --random_shuffle \
        --sort_bulkload=$9 \
        --insert_frac=${10}
done
