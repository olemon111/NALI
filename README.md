# NALI (Numa-Aware persistent Learned-Index)
## Design
  dram index + numa local shread hash nvm log

  specific log design for background gc
  
  log store key and value

  thread pool accelerate range scan

  proxy thread for remote read?(may be no need, 测试时有重复key)

## Operations
  - insert op：
    1. write log
    2. dram index insert <key, cur_log_offset>
  - update op:
    1. get old_log_offset
    2. write log
    2. dram index update <key, old_log_offset> to <key, new_log_offset>
  - get op:
    1. get log_offset
    2. get real_pmem_addr, then get real value
  - delete op:
    1. get old_log_offset
    2. write log
    2. dram delete key
  - scan op:
    ```c++
    for (key: range keys) {
      1. get key log_offset
      2. get real_pmem_addr
      3. send to thread_pool to get value from nvm?
    }
    ```
## TODO for optimize
  - jemalloc optimize dram index?
    - 好像米有效果
  - memory cacheline align?
    - 在使用jemalloc中确保64B对齐了
  - small-grained lock?
    - 性能更差
  - hugepage?
    - 有一点点提升
    ```shell
    mkdir -p /mnt/hugetlbfs
    mount -t hugetlbfs none /mnt/hugetlbfs
    hugeadm --pool-pages-min 2MB:5120
    hugeadm --pool-pages-max 2MB:10240
    hugeadm --pool-list

    LD_PRELOAD=libhugetlbfs.so HUGETLB_MORECORE=yes ./your_exe
    ```
  - thread pool for scan optimize
    - 性能更差
    - 注意：线程池函数不能传atomic变量，会编译fail
# 测试框架
from apex source code
使用apex测试集

 - fastfair使用wipe的fastfair的范围查找函数
 - alexol要先加载10000000的数据
   - 调参叶节点设置为512KB读写性能较好
   - 测alexol注意不要测到扩展时候的性能(例如1亿kv时)
   - lgn数据集只测前130000000性能


## operation
- log struct:
```
8B value's log:
|--2bits--|--62bits--|--8B--|---8B---|--------8B----------|
|   op    |  version |  key |  value | last_log_addr_info |

last_log_addr_info struct:
|---2B--|----1B----|------1B-----|---4B---|
| vlen  |  numa_id | log_page_id | offset |

dram一个global_table存储所有log page的<log_page_id, offset>的映射

var value's log:
|--2bits--|--62bits--|--8B--|--2B--|---------8B----------|--(vlen)B--|
|   op    |  version |  key | vlen |  last_log_addr_info |   value   |
```
- insert(key, value)
```
key_hash = hash(key);
log_addr = alloc_log(key_hash, thread_numa_id, vlen); (每个log为一个page，分配时使用写锁互斥)
在dram index内部，如果是update操作，首先获取old value即old log_addr
alloc version，写log并持久化，然后insert/update <key, log_addr> into dram index
```

- get
```
get <key, log_addr> from dram index
根据log_addr解析出tuple<veln, numa_id, log_page_id, offset>
根据tuple的log_page_id查global_table得到log_page的起始地址
根据veln读出value返回
```

- pactree和dptree有冲突，不要一起make

- TODO
  - pactree mixupdate, mixinsert
  - nali var value length test(proxy test)
  - fastfair 2-32
  - nap test
  - nali var value lenght recovery
  - nvm dram use