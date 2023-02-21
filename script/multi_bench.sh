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

    rm -rf /mnt/pmem0/zzy/*
    rm -rf /mnt/pmem1/zzy/*
    Loadname="ycsb-400m"
    loadnum=0
    date | tee multi-${dbname}-${Loadname}-th${thread}.txt
    # gdb --args \
    # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    # numactl --cpunodebind=1 --membind=1 \
    LD_PRELOAD=libhugetlbfs.so HUGETLB_MORECORE=yes ${BUILDDIR}nali_multi_bench --dbname ${dbname} \
        --loadstype 3 --load-size ${loadnum} --valuesize ${valuesize} --put-size ${opnum} --get-size ${opnum} \
        --numa0-thread $thread --numa1-thread $thread | tee -a multi-${dbname}-${Loadname}-th${thread}.txt
    echo "----------------"
    # sleep 20

    # rm -rf /mnt/pmem0/zzy/*
    # rm -rf /mnt/pmem1/zzy/*
    # Loadname="longlat-400m"
    # loadnum=400000000
    # date | tee multi-${dbname}-${Loadname}-th${thread}.txt
    # # gdb --args \
    # # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    # # numactl --cpunodebind=1 --membind=1 \
    # ${BUILDDIR}nali_multi_bench --dbname ${dbname} \
    #     --loadstype 4 --load-size ${loadnum} --put-size ${opnum} --get-size ${opnum} \
    #     --numa0-thread $thread --numa1-thread $thread | tee -a multi-${dbname}-${Loadname}-th${thread}.txt
    # echo "----------------"
    # sleep 20

    # rm -rf /mnt/pmem0/zzy/*
    # rm -rf /mnt/pmem1/zzy/*
    # loadnum=200000000
    # Loadname="longtitudes-200m"
    # date | tee multi-${dbname}-${Loadname}-th${thread}.txt
    # # gdb --args \
    # # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    # # numactl --cpunodebind=1 --membind=1 \
    # ${BUILDDIR}nali_multi_bench --dbname ${dbname} \
    #     --loadstype 5 --load-size ${loadnum} --put-size ${opnum} --get-size ${opnum} \
    #     --numa0-thread $thread --numa1-thread $thread | tee -a multi-${dbname}-${Loadname}-th${thread}.txt
    # echo "----------------"
    # sleep 20

    # rm -rf /mnt/pmem0/zzy/*
    # rm -rf /mnt/pmem1/zzy/*
    # Loadname="lognormal-150m"
    # loadnum=190000000
    # date | tee multi-${dbname}-${Loadname}-th${thread}.txt
    # # gdb --args \
    # # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    # # numactl --cpunodebind=1 --membind=1 \
    # ${BUILDDIR}nali_multi_bench --dbname ${dbname} \
    #     --loadstype 6 --load-size ${loadnum} --put-size ${opnum} --get-size ${opnum} \
    #     --numa0-thread $thread --numa1-thread $thread | tee -a multi-${dbname}-${Loadname}-th${thread}.txt
    # echo "----------------"
}

loadnum=400000000
opnum=10000000
scansize=4000000
dbname="utree"
valuesize=8 # if value size == 8, undef varvalue
# zipfan=0 # if zipfan = 0, is uniform
for thread in 16
do
    Run $dbname $loadnum $opnum $scansize $thread $valuesize
done