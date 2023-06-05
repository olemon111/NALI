#!/bin/bash
BUILDDIR=$(dirname "$0")/../build/
Loadname="longlat-400m"
function Run() {
    dbname=$1
    loadnum=$2
    opnum=$3
    scansize=$4
    thread=$5
    valuesize=$6
    bgthreads=$7
    hashshards=$8

    rm -rf /mnt/pmem0/zzy/*
    rm -rf /mnt/pmem1/zzy/*
    Loadname="ycsb-200m"
    loadnum=190000000
    date | tee multi-${dbname}-${Loadname}-th${thread}-s${valuesize}-b${bgthreads}-h${hashshards}.txt
    # gdb --args \
    # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    # numactl --cpunodebind=1 --membind=1 \
    # LD_PRELOAD=libhugetlbfs.so HUGETLB_MORECORE=yes \
    timeout 1200 ${BUILDDIR}nali_multi_bench --dbname ${dbname} \
        --loadstype 3 --load-size ${loadnum} --valuesize ${valuesize} --put-size ${opnum} --get-size ${opnum} \
        --thread-nums $thread --bgthreads $bgthreads --hashshards $hashshards | tee -a multi-${dbname}-${Loadname}-th${thread}-s${valuesize}-b${bgthreads}-h${hashshards}.txt
    echo "----------------"
    sleep 5

    # rm -rf /mnt/pmem0/zzy/*
    # rm -rf /mnt/pmem1/zzy/*
    # Loadname="longlat-200m"
    # loadnum=190000000
    # date | tee multi-${dbname}-${Loadname}-th${thread}-s${valuesize}-b${bgthreads}-h${hashshards}.txt
    # # gdb --args \
    # # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    # # numactl --cpunodebind=1 --membind=1 \
    # # LD_PRELOAD=libhugetlbfs.so HUGETLB_MORECORE=yes \
    # timeout 1200 ${BUILDDIR}nali_multi_bench --dbname ${dbname} \
    #     --loadstype 4 --load-size ${loadnum} --valuesize ${valuesize} --put-size ${opnum} --get-size ${opnum} \
    #     --thread-nums $thread --bgthreads $bgthreads --hashshards $hashshards | tee -a multi-${dbname}-${Loadname}-th${thread}-s${valuesize}-b${bgthreads}-h${hashshards}.txt
    # echo "----------------"
    # sleep 5

    # rm -rf /mnt/pmem0/zzy/*
    # rm -rf /mnt/pmem1/zzy/*
    # Loadname="longtitudes-200m"
    # loadnum=190000000
    # date | tee multi-${dbname}-${Loadname}-th${thread}-s${valuesize}-b${bgthreads}-h${hashshards}.txt
    # # gdb --args \
    # # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    # # numactl --cpunodebind=1 --membind=1 \
    # # LD_PRELOAD=libhugetlbfs.so HUGETLB_MORECORE=yes \
    # timeout 1200 ${BUILDDIR}nali_multi_bench --dbname ${dbname} \
    #     --loadstype 5 --load-size ${loadnum} --valuesize ${valuesize} --put-size ${opnum} --get-size ${opnum} \
    #     --thread-nums $thread --bgthreads $bgthreads --hashshards $hashshards | tee -a multi-${dbname}-${Loadname}-th${thread}-s${valuesize}-b${bgthreads}-h${hashshards}.txt
    # echo "----------------"
    # sleep 5

    # rm -rf /mnt/pmem0/zzy/*
    # rm -rf /mnt/pmem1/zzy/*
    # Loadname="lognormal-100m"
    # loadnum=90000000
    # date | tee multi-${dbname}-${Loadname}-th${thread}-s${valuesize}-b${bgthreads}-h${hashshards}.txt
    # # gdb --args \
    # # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    # # numactl --cpunodebind=1 --membind=1 \
    # # LD_PRELOAD=libhugetlbfs.so HUGETLB_MORECORE=yes \
    # timeout 1200 ${BUILDDIR}nali_multi_bench --dbname ${dbname} \
    #     --loadstype 6 --load-size ${loadnum} --valuesize ${valuesize} --put-size ${opnum} --get-size ${opnum} \
    #     --thread-nums $thread --bgthreads $bgthreads --hashshards $hashshards | tee -a multi-${dbname}-${Loadname}-th${thread}-s${valuesize}-b${bgthreads}-h${hashshards}.txt
    # echo "----------------"
    # sleep 5
}

loadnum=400000000
opnum=10000000
scansize=4000000
dbname="alexol"
bgthreads=0
valsize=8

hashshards=128

# alexol var value
# function run_all() {
#     dbs="alexol"
#     for dbname in $dbs; do
#         for bgthreads in 4 8 12 16
#         do
#             for valsize in 8
#             do
#                 for thread in 4 8 12 16
#                 do
#                     Run $dbname $loadnum $opnum $scansize $thread $valsize $bgthreads $hashshards
#                 done
#             done
#         done
#     done
# }


# alexol var hashshards 
function run_all() {
    dbs="nap"
    for dbname in $dbs; do
        for bgthreads in 2 # 4 8 16
        do
            for thread in 16
            do
                Run $dbname $loadnum $opnum $scansize $thread $valsize $bgthreads $hashshards
            done
        done
    done
}

# function run_all() {
#     dbs="alexol"
#     for dbname in $dbs; do
#         for thread in 16
#         do
#             Run $dbname $loadnum $opnum $scansize $thread $valsize
#         done
#     done
# }

run_all $dbname $loadnum $opnum $scansize $thread