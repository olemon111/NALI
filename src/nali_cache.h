// general shard-lrucache, for cross-NUMA write-read mix workload
// insert operations do not go through cache
// read/update operation first go through cache,
    // and then update update dram index and write nvm log
// scan operation directly go through the dram index and nvm log

// TODO: 无锁读！不然无法平衡lru加锁的开销
#pragma once

#include <unordered_map>
#include "util/utils.h"
namespace nali {

// #define USE_NALI_CACHE

#ifdef USE_NALI_CACHE

// cache_size = NALI_CACHE_SHARD_NUMS * NALI_PER_CACHE_SHARD_SLOTS * sizeof(nali_cache_entry)
#define NALI_CACHE_SHARD_NUMS 10240
#define NALI_PER_CACHE_SHARD_SLOTS 32

template <typename T, typename P>
class nali_cache_entry {
    public:
        nali_cache_entry() : is_valid(false), prev_(nullptr), next_(nullptr) {}
        nali_cache_entry(const T& key, const P& value) : key_(key), value_(value), is_valid(true) {}
        void set_new_data(const T& key, const P& value) {
            key_ = key;
            value_ = value;
            is_valid = true;
        }

    public:
        T key_;
        P value_;
        bool is_valid;
        nali_cache_entry<T, P> *prev_;
        nali_cache_entry<T, P> *next_;
};

template <typename T, typename P>
class nali_shard_lrucache {
    public:
        nali_shard_lrucache() : entry_nums_(0), head_(nullptr), tail_(nullptr) {}

        ~nali_shard_lrucache() {
            while (head_) {
                nali_cache_entry<T, P> *next = head_->next_;
                delete head_;
                head_ = next;
            }
        }

        void insert(const T& key, const P& payload) {
            rwlock_.lock();
            auto iter = map_.find(key);
            if (iter == map_.end()) {
                nali_cache_entry<T, P> *node = nullptr;
                if (entry_nums_ < NALI_PER_CACHE_SHARD_SLOTS) {
                    node = new nali_cache_entry<T, P>(key, payload);
                    if (unlikely(entry_nums_ == 0)) {
                        head_ = node;
                        tail_ = head_;
                    } else {
                        node->next_ = head_;
                        head_->prev_ = node;
                        head_ = node;
                    }
                    entry_nums_++;
                }  else {
                    node = evict();
                    node->set_new_data(key, payload);
                    move_to_front(node);
                }
                map_[key] = node;
            } else {
                nali_cache_entry<T, P> *node = iter->second;
                node->set_new_data(key, payload);
                move_to_front(node);
            }
            rwlock_.unlock();
        }
        
        void update(const T& key, const P& payload) {
            insert(key, payload);
        }

        bool search(const T& key, P* payload) {
            rwlock_.lock();
            auto iter = map_.find(key);
            if (iter == map_.end()) {
                rwlock_.unlock();
                return false;
            } else {
                nali_cache_entry<T, P> *node = iter->second;
                *payload = node->value_;
                move_to_front(node);
                rwlock_.unlock();
            }
            return true;
        }

        void erase(const T& key) {
            rwlock_.lock();
            auto iter = map_.find(key);
            if (iter != map_.end()) {
                nali_cache_entry<T, P> *node = iter->second;
                node->is_valid = false;
                map_.erase(iter);
            }
            rwlock_.unlock();
            return;
        }
    
    private:
        // update / search
        void move_to_front(nali_cache_entry<T, P> *node) {
            if (likely(entry_nums_ != 1)) {
                if (unlikely(node == tail_)) {
                    tail_ = node->prev_;
                    tail_->next_ = nullptr;
                    node->prev_ = nullptr;
                    node->next_ = head_;
                    head_->prev_ = node;
                    head_ = node;
                } else if (likely(node != head_)) {
                    node->prev_->next_ = node->next_;
                    node->next_->prev_ = node->prev_;
                    node->prev_ = nullptr;
                    node->next_ = head_;
                    head_->prev_ = node;
                    head_ = node;
                }
            }
        }

        // update / search
        // if node is valid, remove from map
        // not minus entry_nums_, change
        nali_cache_entry<T, P> *evict() {
            assert(entry_nums_ == NALI_PER_CACHE_SHARD_SLOTS);
            nali_cache_entry<T, P> *node = tail_;
            tail_ = node->prev_;
            tail_->next_ = nullptr;
            if (node->is_valid) {
                map_.erase(node->key_);
                node->is_valid = false;
            }
            return node;
        }

    private:
        shared_mutex_u8 rwlock_;
        size_t entry_nums_;
        nali_cache_entry<T, P> *head_;
        nali_cache_entry<T, P> *tail_;
        std::unordered_map<T, nali_cache_entry<T, P>*> map_;
};

template <typename T, typename P>
class nali_lrucache {
    public:
        nali_lrucache() {
            for (int i = 0; i < NALI_CACHE_SHARD_NUMS; i++) {
                cache_[i] = new nali_shard_lrucache<T, P>();
            }
        }

        ~nali_lrucache() {
            delete [] cache_;
        }

        void insert(const T& key, const P& payload, uint64_t key_hash) {
            return;
            // may should not support insert operation
            cache_[key_hash % NALI_CACHE_SHARD_NUMS]->insert(key, payload);
        }

        void update(const T& key, const P& payload, uint64_t key_hash) {
            cache_[key_hash % NALI_CACHE_SHARD_NUMS]->update(key, payload);
        }

        bool search(const T& key, P* payload, uint64_t key_hash) {
            return cache_[key_hash % NALI_CACHE_SHARD_NUMS]->search(key, payload);
        }

        void erase(const T& key, uint64_t key_hash) {
            cache_[key_hash % NALI_CACHE_SHARD_NUMS]->erase(key);
        }
    private:
        nali_shard_lrucache<T, P> *cache_[NALI_CACHE_SHARD_NUMS] = {0};
};

#endif
}