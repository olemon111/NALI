# utree PiBench Wrapper

PiBench wrapper for utree.

## Installation

```bash
mkdir build
cd build 
cmake ..
make
```


## This repo contains souce code from https://github.com/thustorage/nvm-datastructure.git 

commit c57b27f92baf9737e291b29378090da8feb166ce

Modifications:
1. Added update, scan, new_remove (utree.h)
2. zzy: 多线程有bug, insert()和delete()，retry超过次数直接return


