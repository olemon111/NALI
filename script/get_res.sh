#!/bin/bash

# function scalability_get_nvm_write()
# {
#     echo $1 $2
#     cat $1 | grep "NVM WRITE :" | awk '{print $4/(16*10000000)}'
# }

# function get_write_ampli() 
# {
#     echo $1
#     cat $1 | grep "write ampli:" | awk '{print $3*4}'
# }

# function get_dram_size()
# {
#     echo $1
#     cat $1 | grep "dram space use:" | awk '$4>"0.1"{print $4}'
# }

function get_numa0_read()
{
    if cat $1 | grep -q "pmem0, bytes_read:" ; then
        cat $1 | grep "pmem0, bytes_read:" | awk '{print strtonum("0x"$3)/1e7}'
    else
        echo "0"
    fi
}

function get_numa0_write()
{
    if cat $1 | grep -q "pmem0, bytes_written:" ; then
        cat $1 | grep "pmem0, bytes_written:" | awk '{print strtonum("0x"$3)/1e7}'
    else
        echo "0"
    fi
}

function get_recovery_time()
{
    cat $1 | grep "Recovery" | awk '{print $3}'
}

function get_multi_load()
{
    if cat $1 | grep -q "Load" ; then
        cat $1 | grep "Load" | awk '{print $7/1e+06}'
    else
        echo "0"
    fi
}

function get_multi_put()
{
    if cat $1 | grep -q "Put" ; then
        cat $1 | grep "Put" | awk '{print $7/1e+06}'
    else
        echo "0"
    fi
}

function get_multi_get()
{
    if cat $1 | grep -q "Get" ; then
        cat $1 | grep "Get" | awk '{print $7/1e+06}'
    else
        echo "0"
    fi
}

function get_multi_scan()
{
    if cat $1 | grep -q "Scan" ; then
        cat $1 | grep "Scan" | awk '{print $11/1e+06}'
    else
        echo "0"
    fi
}

function get_multi_delete()
{
    if cat $1 | grep -q "Delete" ; then
        cat $1 | grep "Delete" | awk '{print $7/1e+06}'
    else
        echo "0"
    fi
}

function get_multi_update()
{
    if cat $1 | grep -q "Update" ; then
        cat $1 | grep "Update" | awk '{print $7/1e+06}'
    else
        echo "0"
    fi
}

function get_mix_op()
{
    if cat $1 | grep -q $2 ; then
        cat $1 | grep -w $2 | awk '{print $7/1e+06}'
    else
        echo "0"
    fi
}

function get_zipfan_get()
{
    if cat $1 | grep -q "zipfan_read"; then
        cat $1 | grep "zipfan_read"| grep -w $2 | awk '{print $7/1e+06}'
    else
        echo "0"
    fi
}

function get_zipfan_update()
{
    if cat $1 | grep -q "zipfan_update" ; then
        cat $1 | grep "zipfan_update" | grep -w $2 | awk '{print $7/1e+06}'
    else
        echo "0"
    fi
}

function get_nap_local_visit()
{
    if cat $1 | grep -q "local" ; then
        cat $1 | grep "local" | grep -w $2 | awk '{print $3}'
    else
        echo "0"
    fi
}

function get_nap_remote_visit()
{
    if cat $1 | grep -q "remote" ; then
        cat $1 | grep "remote" | grep -w $2 | awk '{print $3}'
    else
        echo "0"
    fi
}

function get_nap_read_iops()
{
    if cat $1 | grep -q "zipfan_read" ; then
        cat $1 | grep "zipfan_read" | grep -w $2 | awk '{print $7/1e+06}'
    else
        echo "0"
    fi
}

function get_nap_update_iops()
{
    if cat $1 | grep -q "zipfan_update" ; then
        cat $1 | grep "zipfan_update" | grep -w $2 | awk '{print $7/1e+06}'
    else
        echo "0"
    fi
}

function get_cache_miss_ratio()
{
    if cat $1 | grep -q "hit rate"; then
        cat $1 | grep "hit rate" | awk '{print 1-$3}'
    else
        echo "0"
    fi
}


dbname=nap
dbname=alexol
# dbname=btreeolc
workload=ycsb-200m
# workload=longlat-200m
# workload=longtitudes-200m
# workload=lognormal-100m

# logfile="microbench-$dbname-$workload.txt"

# for thread in 16
# # for thread in {1..16}
# do
#     for theta in 0.5 0.6 0.7 0.8 0.9 0.99
#     do 
#         logfile="multi-$dbname-$workload-th$thread-s8-b2-h128-zt$theta.txt"
#         get_zipfan_get $logfile "zipfan$theta"
#     done
#     # get_numa0_read $logfile
#     # get_numa0_write $logfile
#     # get_multi_load $logfile
#     # get_multi_put $logfile
#     # get_multi_get $logfile
#     # get_multi_scan $logfile
#     # get_multi_delete $logfile
#     # get_multi_update $logfile

#     # get_mix_op $logfile "Mixupdate0.05"
#     # get_mix_op $logfile "Mixupdate0.2"
#     # get_mix_op $logfile "Mixupdate0.5"
#     # get_mix_op $logfile "Mixupdate0.8"
#     # get_mix_op $logfile "Mixupdate0.95"

#     # get_mix_op $logfile "Mixinsert0.05"
#     # get_mix_op $logfile "Mixinsert0.2"
#     # get_mix_op $logfile "Mixinsert0.5"
#     # get_mix_op $logfile "Mixinsert0.8"
#     # get_mix_op $logfile "Mixinsert0.95"

#     # get_zipfan_update $logfile "zipfan0.99"
#     # get_zipfan_update $logfile "zipfan0.9"
#     # get_zipfan_update $logfile "zipfan0.8"
#     # get_zipfan_update $logfile "zipfan0.7"
#     # get_zipfan_update $logfile "zipfan0.6"
#     # get_zipfan_update $logfile "zipfan0.5"

#     # get_zipfan_get $logfile "zipfan0.99"
#     # get_zipfan_get $logfile "zipfan0.9"
#     # get_zipfan_get $logfile "zipfan0.8"
#     # get_zipfan_get $logfile "zipfan0.7"
#     # get_zipfan_get $logfile "zipfan0.6"
#     # get_zipfan_get $logfile "zipfan0.5"

#     # get_recovery_time $logfile
# done


for thread in 16
# for thread in {1..16}
do
    for theta in 0.5 0.6 0.7 0.8 0.9 0.99
    do 
        logfile="multi-$dbname-$workload-th$thread-s8-b2-h128-zt$theta.txt"
        # get_zipfan_get $logfile "zipfan$theta"
        get_cache_miss_ratio $logfile
    done
done

# echo "update"

# for thread in 16
# # for thread in {1..16}
# do
#     for theta in 0.5 0.6 0.7 0.8 0.9 0.99
#     do 
#         logfile="multi-$dbname-$workload-th$thread-s8-b2-h128-zt$theta.txt"
#         get_zipfan_update $logfile "zipfan$theta"
#     done
# done