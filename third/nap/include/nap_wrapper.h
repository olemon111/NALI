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

struct FastFairTreeIndex {
  fastfair::btree *map;

  FastFairTreeIndex(fastfair::btree *map) : map(map) {}

  void put(const nap::Slice &key, const nap::Slice &value, bool is_update) {
    map->btree_insert((char *)key.data(), (char *)(cur_value++));
  }

  bool get(const nap::Slice &key, std::string &value) {
    uint64_t *ret =
        reinterpret_cast<uint64_t *>(map->btree_search((char *)key.data()));
    return ret != nullptr;
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
private:
    fastfair::btree *ff = nullptr;
    nap::Nap<FastFairTreeIndex> *tree_ = nullptr;
    thread_local static bool thread_init;
};

nap_fastfair_wrapper::nap_fastfair_wrapper() {
  init_numa_pool();
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
  std::cout << "nap warm up" << std::endl;
  tree_ = new nap::Nap<FastFairTreeIndex>(raw_index);
  // nap测试中，首先进行了一段的warmup:执行get操作
  for (int i = 0; i < 100000; i++) {
    std::string str;
    uint64_t key = random();
    tree_->get(nap::Slice((char *)(&key), KEY_LEN), str);
  }
  // nap::Topology::reset();
  tree_->set_sampling_interval(32); // nap默认设置32
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
  // thread_local static char buf_2[4096];
  // fastfair_nap.internal_query(
  //     (nap::Slice((char *)op.key, KEY_LEN)), 10, (char *)buf_2);

  // int off = 0;
  // tree->btree_search_range((char *)op.key, max_str,
  //                           (unsigned long *)thread_local_buffer,
  //                                        10, off);
  // TODO
  return 0;
}
