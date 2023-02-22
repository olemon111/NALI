#ifndef __DPTREE_WRAPPER_HPP__
#define __DPTREE_WRAPPER_HPP__

#include <btreeolc.hpp>
#include <concur_dptree.hpp>

#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <libpmemobj.h>
#include <atomic>

// #define DEBUG_MSG
extern int parallel_merge_worker_num;
thread_local char k_[128];
thread_local uint64_t vkcmp_time = 0;
std::atomic<uint64_t> pmem_lookup;

class dptree_wrapper
{
public:
    dptree_wrapper(int worker_num);
    virtual ~dptree_wrapper();
    
    virtual bool find(uint64_t key, uint64_t &value);
    virtual bool insert(uint64_t key, uint64_t value);
    virtual bool update(uint64_t key, uint64_t value);
    virtual bool remove(uint64_t key);
    virtual int scan(uint64_t key, uint32_t to_scan, std::pair<uint64_t, uint64_t>*& values_out);
private:
    dptree::concur_dptree<uint64_t, uint64_t> *dptree;
};

struct KV {
    uint64_t k;
    uint64_t v;
};

dptree_wrapper::dptree_wrapper(int worker_num)
{
#ifdef PROFILE
    pmem_lookup.store(0);
#endif
    parallel_merge_worker_num = worker_num;
    dptree = new dptree::concur_dptree<uint64_t, uint64_t>();
}

dptree_wrapper::~dptree_wrapper()
{
#ifdef PROFILE
    printf("vkcmp time: %llu\n", vkcmp_time);
    printf("pmem lookup: %llu\n", pmem_lookup.load());
#endif
}

bool dptree_wrapper::find(uint64_t key, uint64_t &value) {
    dptree->lookup(key, value);
    value = value >> 1;
    return true;
}

bool dptree_wrapper::insert(uint64_t key, uint64_t value) {
    dptree->insert(key, value);
    return true;
}

bool dptree_wrapper::update(uint64_t key, uint64_t value) {
    dptree->upsert(key, value);
    return true;
}

bool dptree_wrapper::remove(uint64_t key) {
    // current dptree code does not implement delete
    return true;
}

int dptree_wrapper::scan(uint64_t key, uint32_t to_scan, std::pair<uint64_t, uint64_t>*& values_out) {
    static thread_local std::vector<uint64_t> v(to_scan*2);
    v.clear();
    dptree->scan(key, to_scan, v);
    // values_out = (char*)v.data();
    return v.size() / 2;
}

#endif
