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

    # rm -rf /mnt/pmem0/zzy/*
    # rm -rf /mnt/pmem1/zzy/*
    Loadname="ycsb-200m"
    loadnum=190000000
    date | tee multi-${dbname}-${Loadname}-th${thread}.txt
    # gdb --args \
    # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    # numactl --cpunodebind=1 --membind=1 \
    # LD_PRELOAD=libhugetlbfs.so HUGETLB_MORECORE=yes \
    timeout 1200 ${BUILDDIR}nali_multi_bench --dbname ${dbname} \
        --loadstype 3 --load-size ${loadnum} --valuesize ${valuesize} --put-size ${opnum} --get-size ${opnum} \
        --thread-nums $thread | tee -a multi-${dbname}-${Loadname}-th${thread}.txt
    echo "----------------"
    sleep 10

    # rm -rf /mnt/pmem0/zzy/*
    # rm -rf /mnt/pmem1/zzy/*
    # Loadname="longlat-200m"
    # loadnum=190000000
    # date | tee multi-${dbname}-${Loadname}-th${thread}.txt
    # # gdb --args \
    # # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    # # numactl --cpunodebind=1 --membind=1 \
    # # LD_PRELOAD=libhugetlbfs.so HUGETLB_MORECORE=yes \
    # timeout 1200 ${BUILDDIR}nali_multi_bench --dbname ${dbname} \
    #     --loadstype 4 --load-size ${loadnum} --valuesize ${valuesize} --put-size ${opnum} --get-size ${opnum} \
    #     --thread-nums $thread | tee -a multi-${dbname}-${Loadname}-th${thread}.txt
    # echo "----------------"
    # sleep 10

    # rm -rf /mnt/pmem0/zzy/*
    # rm -rf /mnt/pmem1/zzy/*
    # Loadname="longtitudes-200m"
    # loadnum=190000000
    # date | tee multi-${dbname}-${Loadname}-th${thread}.txt
    # # gdb --args \
    # # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    # # numactl --cpunodebind=1 --membind=1 \
    # # LD_PRELOAD=libhugetlbfs.so HUGETLB_MORECORE=yes \
    # timeout 1200 ${BUILDDIR}nali_multi_bench --dbname ${dbname} \
    #     --loadstype 5 --load-size ${loadnum} --valuesize ${valuesize} --put-size ${opnum} --get-size ${opnum} \
    #     --thread-nums $thread | tee -a multi-${dbname}-${Loadname}-th${thread}.txt
    # echo "----------------"
    # sleep 10

    # rm -rf /mnt/pmem0/zzy/*
    # rm -rf /mnt/pmem1/zzy/*
    # Loadname="lognormal-100m"
    # loadnum=90000000
    # date | tee multi-${dbname}-${Loadname}-th${thread}.txt
    # # gdb --args \
    # # LD_PRELOAD="../build/pmdk/src/PMDK/src/nondebug/libpmemobj.so.1"\
    # # numactl --cpunodebind=1 --membind=1 \
    # # LD_PRELOAD=libhugetlbfs.so HUGETLB_MORECORE=yes \
    # timeout 1200 ${BUILDDIR}nali_multi_bench --dbname ${dbname} \
    #     --loadstype 6 --load-size ${loadnum} --valuesize ${valuesize} --put-size ${opnum} --get-size ${opnum} \
    #     --thread-nums $thread | tee -a multi-${dbname}-${Loadname}-th${thread}.txt
    # echo "----------------"
    # sleep 10
}

loadnum=400000000
opnum=10000000
scansize=4000000
dbname="alexol"
# zipfan=0 # if zipfan = 0, is uniform
# for thread in {1..32}
# do
#     Run $dbname $loadnum $opnum $scansize $thread $valuesize
# done

# alexol var value
function run_all() {
    dbs="alexol"
    for dbname in $dbs; do
        for valsize in 64 #64 #128 #256 512 1024
        do
            for thread in 16
            do
                Run $dbname $loadnum $opnum $scansize $thread $valsize
            done
        done
    done
}

# function run_all() {
#     dbs="fastfair"
#     for dbname in $dbs; do
#         for thread in {1..16}
#         do
#             Run $dbname $loadnum $opnum $scansize $thread $valsize
#         done
#     done
# }

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