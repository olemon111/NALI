#!/bin/bash

function get_cdf()
{
    Loadname="ycsb-400m-zipf0.9"
    logfile="multi-${dbname}-${Loadname}-th${thread}.txt"
    result_file="cdf_multi-${dbname}-${Loadname}-th${thread}.txt"
    echo $logfile
    cat $logfile | grep "cdf:" | awk '{print $4}' > $result_file

    Loadname="longlat-400m-zipf0.9"
    logfile="multi-${dbname}-${Loadname}-th${thread}.txt"
    result_file="cdf_multi-${dbname}-${Loadname}-th${thread}.txt"
    echo $logfile
    cat $logfile | grep "cdf:" | awk '{print $4}' > $result_file

    Loadname="longtitudes-200m-zipf0.9"
    logfile="multi-${dbname}-${Loadname}-th${thread}.txt"
    result_file="cdf_multi-${dbname}-${Loadname}-th${thread}.txt"
    echo $logfile
    cat $logfile | grep "cdf:" | awk '{print $4}' > $result_file

    Loadname="lognormal-150m-zipf0.9"
    logfile="multi-${dbname}-${Loadname}-th${thread}.txt"
    result_file="cdf_multi-${dbname}-${Loadname}-th${thread}.txt"
    echo $logfile
    cat $logfile | grep "cdf:" | awk '{print $4}' > $result_file
}


dbname=nali
Loadname="ycsb-400m-zipf0.9"
thread=16
logfile="multi-${dbname}-${Loadname}-th${thread}.txt"

get_cdf dbname Loadname thread