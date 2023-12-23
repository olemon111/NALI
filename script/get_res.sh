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
    if cat $1 | grep -q "Mix$2" ; then
        cat $1 | grep -w "Mix$2" | awk '{print $7/1e+06}'
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

# workload=longtitudes-200m
# workload=lognormal-100m

function get_zipfan_repeat()
{
    dbname=$1
    workload=$2
    repeat=$3
    
    for r in $(seq 1 $repeat)
    do
        echo -e "$r===================="
        # for thread in {1..16}
        for thread in 16
        do
            for theta in 0.5 0.6 0.7 0.8 0.9 0.99
            do
                logfile="multi-$dbname-$workload-th$thread-s8-b2-h128-zt$theta-r$r.txt"
                # get_zipfan_get $logfile "zipfan$theta"
                # get_zipfan_update $logfile "zipfan$theta"
            done
        done
    done
}

function get_repeat()
{
    repeat=$1
    theta=0
    
    for r in $(seq 1 $repeat)
    do
        echo -e "$r===================="
        for thread in {1..16}
        # for thread in 16
        do
            logfile="multi-$dbname-$workload-th$thread-s8-b2-h128-zt$theta-r$r.txt"
            # get_multi_update $logfile
            # get_multi_delete $logfile
            # get_mix_op $logfile "update"
            get_mix_op $logfile "insert"
        done
    done
}



dbname=viper
workload=ycsb-200m
repeat=20

get_repeat $repeat > ../res-ycsb.txt
workload=longlat-200m
get_repeat $repeat > ../res-llt.txt
