#pragma once

// #include "tree_api.hpp"
#include "pactree.h"
#include <numa-config.h>
#include "pactreeImpl.h"

#include <cstring>
#include <mutex>
#include <vector>
#include <shared_mutex>
#include <atomic>
#include <omp.h>

std::atomic<uint64_t> dram_allocated(0);
std::atomic<uint64_t> pmem_allocated(0);
std::atomic<uint64_t> dram_freed(0);
std::atomic<uint64_t> pmem_freed(0);


class pactree_wrapper
{
public:
    pactree_wrapper();
    virtual ~pactree_wrapper();

    virtual bool find(Key_t key, Val_t &value);
    virtual bool insert(Key_t key, const Val_t &value);
    virtual bool update(Key_t key, const Val_t &value);
    virtual bool remove(Key_t key);
    virtual int scan(Key_t key, uint32_t to_scan, std::pair<Key_t, Val_t>*& values_out);
private:
    pactree *tree_ = nullptr;
    thread_local static bool thread_init;
};
//static std::atomic<uint64_t> i_(0);
thread_local bool pactree_wrapper::thread_init = false;
struct ThreadHelper
{
    ThreadHelper(pactree* t){
        t->registerThread();
	// int id = omp_get_thread_num();
        // printf("Thread ID: %d\n", id);
    }
    ~ThreadHelper(){}
    
};

pactree_wrapper::pactree_wrapper()
{
    tree_ = new pactree(1);
}

pactree_wrapper::~pactree_wrapper()
{
#ifdef MEMORY_FOOTPRINT
    // printf("DRAM Allocated: %llu\n", dram_allocated.load());
    // printf("DRAM Freed: %llu\n", dram_freed.load());
    printf("PMEM Allocated: %llu\n", pmem_allocated.load());
    // printf("PMEM Freed: %llu\n", pmem_freed.load());
#endif
    if (tree_ != nullptr)
        delete tree_;
    //tree_ = nullptr;
}

bool pactree_wrapper::find(Key_t key, Val_t &value) {
    thread_local ThreadHelper t(tree_);
    value = tree_->lookup(key);
    return true;
}

bool pactree_wrapper::insert(Key_t key, const Val_t &value) {
    thread_local ThreadHelper t(tree_);
    return tree_->insert(key, value);
}

bool pactree_wrapper::update(Key_t key, const Val_t &value) {
    thread_local ThreadHelper t(tree_);
    return tree_->update(key, value);
}

bool pactree_wrapper::remove(Key_t key) {
    thread_local ThreadHelper t(tree_);
    return tree_->remove(key);
}

int pactree_wrapper::scan(Key_t key, uint32_t to_scan, std::pair<Key_t, Val_t>*& values_out) {
    thread_local ThreadHelper t(tree_);
    thread_local std::vector<Val_t> results;
    results.reserve(to_scan);
    int scanned = tree_->scan(key, to_scan, results);
    return scanned;
}
