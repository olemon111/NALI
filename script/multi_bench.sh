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
    theta=$9
    repeat=${10}

    rm -rf /mnt/pmem0/zzy/*
    rm -rf /mnt/pmem1/zzy/*
    Loadname="ycsb-200m"
    loadnum=190000000 # alexol bulk 10000000
    # loadnum=200000000
    date | tee multi-${dbname}-${Loadname}-th${thread}-s${valuesize}-b${bgthreads}-h${hashshards}-zt${theta}-r${repeat}.txt
    # gdb --args \
    # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    # numactl --cpunodebind=1 --membind=1 \
    # LD_PRELOAD=libhugetlbfs.so HUGETLB_MORECORE=yes \
    timeout 1200 ${BUILDDIR}nali_multi_bench --dbname ${dbname} \
        --loadstype 3 --load-size ${loadnum} --valuesize ${valuesize} --put-size ${opnum} --get-size ${opnum} \
        --thread-nums $thread --bgthreads $bgthreads --hashshards $hashshards --zipfan-theta ${theta} | tee -a multi-${dbname}-${Loadname}-th${thread}-s${valuesize}-b${bgthreads}-h${hashshards}-zt${theta}-r${repeat}.txt
    echo "----------------"
    sleep 10

    # rm -rf /mnt/pmem0/zzy/*
    # rm -rf /mnt/pmem1/zzy/*
    # Loadname="longlat-200m"
    # loadnum=190000000
    # # loadnum=200000000
    # date | tee multi-${dbname}-${Loadname}-th${thread}-s${valuesize}-b${bgthreads}-h${hashshards}-zt${theta}-r${repeat}.txt
    # # gdb --args \
    # # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    # # numactl --cpunodebind=1 --membind=1 \
    # # LD_PRELOAD=libhugetlbfs.so HUGETLB_MORECORE=yes \
    # timeout 1200 ${BUILDDIR}nali_multi_bench --dbname ${dbname} \
    #     --loadstype 4 --load-size ${loadnum} --valuesize ${valuesize} --put-size ${opnum} --get-size ${opnum} \
    #     --thread-nums $thread --bgthreads $bgthreads --hashshards $hashshards --zipfan-theta ${theta} | tee -a multi-${dbname}-${Loadname}-th${thread}-s${valuesize}-b${bgthreads}-h${hashshards}-zt${theta}-r${repeat}.txt
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
theta=0

# alexol var hashshards 
function run_all_zipfan() {
    dbs="viper"
    for dbname in $dbs; do
        for bgthreads in 2 # 4 8 16
        do
            for repeat in {1..10}
            do
                # for thread in {1..16}
                for thread in 16
                do
                    for theta in 0.5 0.6 0.7 0.8 0.9 0.99
                    do
                        Run $dbname $loadnum $opnum $scansize $thread $valsize $bgthreads $hashshards $theta $repeat
                    done
                done
            done
        done
    done
}

function run_all() {
    dbs="viper"
    dbs="viper_cceh2alexol"
    repeat=$1

    for dbname in $dbs; do
        for bgthreads in 2 # 4 8 16
        do
            for repeat in $(seq 1 $repeat)
            do
                for thread in {1..16}
                do
                    Run $dbname $loadnum $opnum $scansize $thread $valsize $bgthreads $hashshards $theta $repeat
                done
            done
        done
    done
}

run_all 10