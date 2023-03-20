#!/bin/bash
./build.sh
BUILDDIR=$(dirname "$0")/build/
Loadname="longlat-400m"
function Run() {
    dbname=$1
    loadnum=$2
    opnum=$3
    scansize=$4
    thread=$5
    valuesize=$6

    Loadname="ycsb-200m"
    loadnum=30000000
    date | tee recovery-${dbname}-${Loadname}-th${thread}-s${valuesize}-b${bgthreads}-h${hashshards}.txt
    # gdb --args \
    # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    # numactl --cpunodebind=1 --membind=1 \
    # LD_PRELOAD=libhugetlbfs.so HUGETLB_MORECORE=yes \
    timeout 1200 ${BUILDDIR}nali_multi_bench --dbname ${dbname} \
        --loadstype 3 --load-size ${loadnum} --valuesize ${valuesize} --put-size ${opnum} --get-size ${opnum} \
        --thread-nums $thread --bgthreads $bgthreads --hashshards $hashshards | tee -a recovery-${dbname}-${Loadname}-th${thread}-s${valuesize}-b${bgthreads}-h${hashshards}.txt
    echo "----------------"
    sleep 5
}

loadnum=400000000
opnum=10000000
scansize=4000000
dbname="alexol"
valsize=8 # 8 16 32 64 128 256 512 1024

function run_all() {
    dbs="alexol"
    for dbname in $dbs; do
        for thread in 1 2 4 6 8 12 16 20 28 32
        do
            Run $dbname $loadnum $opnum $scansize $thread $valsize
        done
    done
}

run_all $dbname $loadnum $opnum $scansize $thread