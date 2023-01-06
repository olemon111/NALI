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
      3. send to thread_pool to get value from nvm
    }
    ```
## TODO for optimize
  - jemalloc optimize dram index?
  - memory cacheline align?
  - small-grained lock?
  - hugepage?

# 测试框架
from apex source code
使用apex测试集

 - fastfair使用wipe的fastfair的范围查找函数
 - alexol要先加载10000000的数据
   - 调参叶节点设置为512KB读写性能较好
   - 测alexol注意不要测到扩展时候的性能(例如1亿kv时)
   - lgn数据集只测前130000000性能
 - 先基于art进行第一版代码实现
   - 读和范围扫描需要把value所在的page pin住，防止被gc