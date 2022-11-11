#!/bin/bash
BUILDDIR=$(dirname "$0")/../build/
Loadname="longlat-400m"
function Run() {
    dbname=$1
    loadnum=$2
    opnum=$3
    scansize=$4
    thread=$5

    rm -rf /mnt/pmem0/zzy/*
    rm -rf /mnt/pmem1/zzy/*
    Loadname="ycsb-400m-zipf0.9"
    date | tee multi-${dbname}-${Loadname}-th${thread}.txt
    # gdb --args \
    # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    # numactl --cpunodebind=1 --membind=1 \
    ${BUILDDIR}multi_bench --dbname ${dbname} \
        --loadstype 3 --load-size ${loadnum} --put-size ${opnum} --get-size ${opnum} \
        -t $thread | tee -a multi-${dbname}-${Loadname}-th${thread}.txt
    echo "----------------"
    # sleep 60

    # rm -rf /mnt/pmem0/zzy/*
    # rm -rf /mnt/pmem1/zzy/*
    # Loadname="longlat-400m-zipf0.9"
    # date | tee multi-${dbname}-${Loadname}-th${thread}.txt
    # # gdb --args \
    # # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    # numactl --cpunodebind=1 --membind=1 \
    # ${BUILDDIR}multi_bench --dbname ${dbname} \
    #     --loadstype 4 --load-size ${loadnum} --put-size ${opnum} --get-size ${opnum} \
    #     -t $thread | tee -a multi-${dbname}-${Loadname}-th${thread}.txt
    # echo "----------------"
    # # sleep 60

    # rm -rf /mnt/pmem0/zzy/*
    # rm -rf /mnt/pmem1/zzy/*
    # Loadname="longtitudes-200m-zipf0.9"
    # date | tee multi-${dbname}-${Loadname}-th${thread}.txt
    # # gdb --args \
    # # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    # numactl --cpunodebind=1 --membind=1 \
    # ${BUILDDIR}multi_bench --dbname ${dbname} \
    #     --loadstype 5 --load-size ${loadnum} --put-size ${opnum} --get-size ${opnum} \
    #     -t $thread | tee -a multi-${dbname}-${Loadname}-th${thread}.txt
    # echo "----------------"
    # # sleep 60

    # rm -rf /mnt/pmem0/zzy/*
    # rm -rf /mnt/pmem1/zzy/*
    # Loadname="lognormal-150m-zipf0.9"
    # date | tee multi-${dbname}-${Loadname}-th${thread}.txt
    # # gdb --args \
    # # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    # numactl --cpunodebind=1 --membind=1 \
    # ${BUILDDIR}multi_bench --dbname ${dbname} \
    #     --loadstype 6 --load-size ${loadnum} --put-size ${opnum} --get-size ${opnum} \
    #     -t $thread | tee -a multi-${dbname}-${Loadname}-th${thread}.txt
    # echo "----------------"
    # # sleep 60
}

loadnum=390000000
opnum=10000000
scansize=4000000
dbname="alexol"
for thread in 16
do
    Run $dbname $loadnum $opnum $scansize $thread
done