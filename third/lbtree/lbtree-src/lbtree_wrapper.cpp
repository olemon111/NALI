#include "lbtree_wrapper.hpp"
#include <chrono>

constexpr const auto MEMPOOL_ALIGNMENT = 4096LL;

size_t key_size_ = 0;

lbtree_wrapper *create_lbtree(int num_threads)
{
  const char *pool_path_ = "/mnt/pmem0/zzy/nali.data";
  size_t pool_size_ = ((size_t)(1024 * 1024 * 32) * 1024);
#ifdef VAR_KEY
  key_size_ = opt.key_size;
  if (key_size_ < 8)
    key_size_ = 8;
  if (key_size_ > 127)
  {
    printf("Variable-length key with lbtree_max length 127!\n");
    exit(1);
  }
  printf("Variable-length key with size: %lld\n", key_size_);
#endif
#ifdef TSX_FAKE
  puts("Using Faked TSX Instructions! Multithreaded not valid");
#endif
  // long long mempool_size = (long long)16 * (long long)1024 * (long long)MB; // 16 GB for inner nodes
  // if (const char *env_p = std::getenv("MEMPOOL"))
  // {
  //   mempool_size = atoll(env_p);
  // }
  initUseful();
  worker_id = 0;
  worker_thread_num = num_threads;
  
  // std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::steady_clock::now();
  the_thread_mempools.init(num_threads, 4096, MEMPOOL_ALIGNMENT);
  // std::chrono::time_point<std::chrono::steady_clock> end = std::chrono::steady_clock::now();
  // std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  // printf("mempools init time: %lld ms \n", ms.count());

#ifdef PMEM
  printf("PMEM Pool Path: %s\n", pool_path_);
  printf("PMEM Pool size: %lld\n", pool_size_);
#endif
  auto start = std::chrono::steady_clock::now();
  the_thread_nvmpools.init(num_threads, pool_path_, pool_size_);
  auto end = std::chrono::steady_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  printf("nvmpools init time: %lld ms \n", ms.count());
  char *nvm_addr = (char *)nvmpool_alloc(256);
  nvmLogInit(num_threads);

  // printf("MemPool Size: %lld, NVMPool Size: %lld. Using file %s\n", mempool_size, nvmpool_size, path_ptr->c_str());
  return new lbtree_wrapper(nvm_addr, false);
}
