# NALI (Numa-Aware persistent Learned-Index)

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