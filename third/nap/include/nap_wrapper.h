#pragma once

#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>

#include <cstdio>
#include <iterator>
#include <sstream>
#include <thread>
#include <time.h>
#include <vector>
#include <string>
#include <gperftools/profiler.h>

#define ENABLE_NAP
// #define GLOBAL_VERSION

// #define USE_GLOBAL_LOCK
// #define RANGE_BENCH

#include "index/fast_fair.h"
#include "nap.h"
#include "slice.h"

#define LAYOUT "NAP_RAW_INDEX"
#define KEY_LEN 8
#define VALUE_LEN 8

thread_local uint64_t cur_value = 1;
thread_local uint8_t thread_local_buffer[4096];
extern char nap_max_str[15];

// extern nap::ThreadMeta thread_meta_array[nap::kMaxThreadCnt];
extern bool zipfan_numa_test_flag;

struct FastFairTreeIndex {
  fastfair::btree *map;

  FastFairTreeIndex(fastfair::btree *map) : map(map) {}

  void put(const nap::Slice &key, const nap::Slice &value, bool is_update) {
    int8_t numa_id = map->btree_insert((char *)key.data(), (char *)(cur_value++));
    if (zipfan_numa_test_flag) {
      nap::thread_meta_array[global_thread_id].ff_numa_write[numa_id]++;
    }
  }

  bool get(const nap::Slice &key, std::string &value) {
    int8_t numa_id = map->btree_search((char *)key.data());
    if (zipfan_numa_test_flag) {
      nap::thread_meta_array[global_thread_id].ff_numa_read[numa_id]++;
    }
    return true;
  }

  void del(const nap::Slice &key) {}
};

class nap_fastfair_wrapper
{
public:
    nap_fastfair_wrapper();
    virtual ~nap_fastfair_wrapper();

    virtual bool find(size_t key, size_t &value);
    virtual bool insert(size_t key, size_t value);
    virtual bool update(size_t key, size_t value);
    virtual bool remove(size_t key);
    virtual int scan(size_t key, uint32_t to_scan, std::pair<size_t, size_t>*& values_out);
    virtual void print_numa_info(bool is_read);
private:
    fastfair::btree *ff = nullptr;
    nap::Nap<FastFairTreeIndex> *tree_ = nullptr;
    thread_local static bool thread_init;
};

nap_fastfair_wrapper::nap_fastfair_wrapper() {
  init_nap_numa_pool();
  ff = new fastfair::btree();
  // nap测试中，先向raw index load部分KV，两边numa都插入一部分KV? 必须要做吗？
  std::cout << "nap insert some kvs into numa0" << std::endl;
  FastFairTreeIndex *raw_index = new FastFairTreeIndex(ff);
  for (int i = 0; i < 500000; i++) {
    global_thread_id = 0;
    uint64_t key = i;
    raw_index->put(nap::Slice((char *)(&key), KEY_LEN), nap::Slice((char *)(&key), VALUE_LEN), false);
  }
  std::cout << "nap insert some kvs into numa1" << std::endl;
  for (int i = 0; i < 500000; i++) {
    global_thread_id = 16;
    uint64_t key = 1000000+i;
    raw_index->put(nap::Slice((char *)(&key), KEY_LEN), nap::Slice((char *)(&key), VALUE_LEN), false);
  }
  std::cout << "nap warm up..." << std::endl;
  tree_ = new nap::Nap<FastFairTreeIndex>(raw_index);
  // nap测试中，首先进行了一段的warmup:执行get操作
  for (int i = 0; i < 100000; i++) {
    std::string str;
    uint64_t key = random();
    tree_->get(nap::Slice((char *)(&key), KEY_LEN), str);
  }
  std::cout << "nap warm up end" << std::endl;
  // nap::Topology::reset();
  tree_->set_sampling_interval(32); // nap默认设置32
  tree_->set_switch_interval(5);
  tree_->clear(); // 统计信息
  std::cout << "nap init end" << std::endl;
}

nap_fastfair_wrapper::~nap_fastfair_wrapper() {
  delete ff;
  delete tree_;
}

bool nap_fastfair_wrapper::find(size_t key, size_t &value) {
  std::string str;
  tree_->get(nap::Slice((char *)(&key), KEY_LEN), str);
  memcpy(&value, str.c_str(), 8);
  return true;
}

bool nap_fastfair_wrapper::insert(size_t key, size_t value) {
  tree_->put(nap::Slice((char *)(&key), KEY_LEN), nap::Slice((char *)(&value), VALUE_LEN), false);
  return true;
}

bool nap_fastfair_wrapper::update(size_t key, size_t value) {
  tree_->put(nap::Slice((char *)(&key), KEY_LEN), nap::Slice((char *)(&value), VALUE_LEN), true);
  return true;
}

bool nap_fastfair_wrapper::remove(size_t key) {
  tree_->del(nap::Slice((char *)(&key), KEY_LEN));
  return true;
}

int nap_fastfair_wrapper::scan(size_t key, uint32_t to_scan, std::pair<size_t, size_t>*& values_out) {
  thread_local static char scan_buf[4096];
  tree_->internal_query((nap::Slice((char *)(&key), KEY_LEN)), to_scan, scan_buf);
  int off = 0;
  ff->btree_search_range(key, UINT64_MAX, (unsigned long *)thread_local_buffer, to_scan, off);
  return 0;
}

void nap_fastfair_wrapper::print_numa_info(bool is_read) {
  uint64_t local_visit = 0, remote_visit = 0;
  for (int i = 0; i < nap::kMaxThreadCnt; i++) {
    int8_t thread_numa_id = numa_map[i];
    if (is_read) {
      local_visit += nap::thread_meta_array[i].ff_numa_read[thread_numa_id];
      remote_visit += nap::thread_meta_array[i].ff_numa_read[1-thread_numa_id];
    } else {
      local_visit += nap::thread_meta_array[i].ff_numa_write[thread_numa_id];
      remote_visit += nap::thread_meta_array[i].ff_numa_write[1-thread_numa_id];
    }
    memset(nap::thread_meta_array[i].ff_numa_read, 0, sizeof(nap::thread_meta_array[i].ff_numa_read));
    memset(nap::thread_meta_array[i].ff_numa_write, 0, sizeof(nap::thread_meta_array[i].ff_numa_write));
  }

  std::cout << "local visit: " << local_visit << std::endl;
  std::cout << "remote visit: " << remote_visit << std::endl;
}