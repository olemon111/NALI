sudo rm -rf /mnt/pmem0/zzy/*
sudo rm -rf /mnt/pmem1/zzy/*

#!/usr/bin/env bash

if [[ "$#" -ne 0 && $1 == "debug" ]]
then
    mkdir -p build_debug;
    cd build_debug;
    cmake -DCMAKE_BUILD_TYPE=Debug ..;
else
    mkdir -p build;
    cd build;
    # cmake -DCMAKE_BUILD_TYPE=Release ..;
    cmake -DCMAKE_BUILD_TYPE=Debug ..;
fi
make;
cd ..;


sudo gdbserver localhost:9555 ./build/nali_multi_bench