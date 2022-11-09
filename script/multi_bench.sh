#!/bin/bash
BUILDDIR=$(dirname "$0")/../build/
Loadname="longlat-400m"
function Run() {
    dbname=$1
    loadnum=$2
    opnum=$3
    scansize=$4
    thread=$5

    rm -rf /mnt/pmem1/zzy/*
    Loadname="ycsb-400m-zipf0.9"
    date | tee multi-${dbname}-${Loadname}-th${thread}.txt
    # gdb --args \
    # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    numactl --cpunodebind=1 --membind=1 \
    ${BUILDDIR}multi_bench --dbname ${dbname} \
        --loadstype 3 --load-size ${loadnum} --put-size ${opnum} --get-size ${opnum} \
        -t $thread | tee -a multi-${dbname}-${Loadname}-th${thread}.txt
    echo "----------------"
    # sleep 60

    # rm -rf /mnt/pmem0/*
    # rm -rf /mnt/pmem1/*
    # Loadname="longlat-400m-zipf0.9"
    # # Loadname="longlat-400m"
    # date | tee multi-${dbname}-${Loadname}-th${thread}.txt
    # # gdb --args \
    # LD_PRELOAD=libhugetlbfs.so HUGETLB_MORECORE=yes timeout 660 ${BUILDDIR}/multi_bench --dbname ${dbname} \
    #     --loadstype 4 --load-size ${loadnum} --put-size ${opnum} --get-size ${opnum} \
    #     -t $thread | tee -a multi-${dbname}-${Loadname}-th${thread}.txt
    # echo "----------------"
    # sleep 60

    # rm -rf /mnt/pmem0/*
    # # Loadname="longtitudes-200m"
    # Loadname="longtitudes-200m-zipf0.9"
    # loadnum=200000000
    # date | tee multi-${dbname}-${Loadname}-th${thread}.txt
    # # gdb --args \
    # timeout 660 numactl --cpubind=0 --membind=0 ${BUILDDIR}/multi_bench --dbname ${dbname} \
    #     --loadstype 5 --load-size ${loadnum} --put-size ${opnum} --get-size ${opnum} \
    #     -t $thread | tee -a multi-${dbname}-${Loadname}-th${thread}.txt
    # echo "----------------"
    # sleep 60

    # rm -rf /mnt/pmem0/*
    # # Loadname="lognormal-150m"
    # Loadname="lognormal-150m-zipf0.9"
    # loadnum=130000000
    # date | tee multi-${dbname}-${Loadname}-th${thread}.txt
    # # gdb --args \
    # timeout 660 numactl --cpubind=0 --membind=0 ${BUILDDIR}/multi_bench --dbname ${dbname} \
    #     --loadstype 6 --load-size ${loadnum} --put-size ${opnum} --get-size ${opnum} \
    #     -t $thread | tee -a multi-${dbname}-${Loadname}-th${thread}.txt
    # echo "----------------"
    # sleep 60
}

loadnum=290000000
opnum=10000000
scansize=4000000
dbname="art"
for thread in 16
do
    Run $dbname $loadnum $opnum $scansize $thread
done