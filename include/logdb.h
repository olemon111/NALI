#pragma once

#include "tree.h"
#include "nali_cache.h"
#include "nali_kvlog.h"
#include "util/utils.h"
#include "util/xxhash.h"
#include "util/concurrentqueue.h"
#include "../src/nali_kvlog.h"

namespace nali {

extern thread_local size_t thread_id;

#ifdef USE_PROXY
extern thread_local size_t random_thread_id[numa_max_node]; 
#endif

#ifdef USE_PROXY
    template <class P>
    struct read_req {
        char *value_ptr;
        bool is_ready;
        P *value;
        read_req() : value_ptr(nullptr), is_ready(false), value(nullptr) {}
        read_req(char *addr, P *paylaod) : value_ptr(addr), is_ready(false), value(paylaod) {}
        void set_req(char *addr, P *paylaod) {
            value_ptr = addr;
            is_ready = false;
            value_ptr = paylaod;
        }
    };
#endif

    template <class T, class P>
    class logdb : public Tree<T, P> {
        public:
            typedef std::pair<T, P> V;
            logdb(Tree <T, uint64_t> *db, const std::vector<std::vector<int>> &thread_arr) : db_(db) {
                log_kv_ = new nali::LogKV<T, P>(thread_arr);
                thread_ids = thread_arr;
                #ifdef USE_NALI_CACHE
                cache_ = new nali::nali_lrucache<T, P>();
                #endif
            }

            ~logdb() {
                delete db_;
                delete log_kv_;
                #ifdef USE_NALI_CACHE
                delete cache_;
                #endif
            }

            void bulk_load(const V values[], int num_keys) {
                std::pair<T, uint64_t> *t_values;
                t_values = new std::pair<T, uint64_t>[num_keys];
                for (int i = 0; i < num_keys; i++) {
                    T key = values[i].first;
                    uint64_t key_hash_ = XXH3_64bits(&key, sizeof(key));
                    uint64_t res = log_kv_->insert(key, values[i].second, key_hash_);
                    t_values[i].first = key;
                    t_values[i].second = res;
                }
                db_->bulk_load(t_values, num_keys);
                delete [] t_values;
            }

            bool insert(const T& key, const P& payload, bool epoch = false) {
                const T kkk = key;
                uint64_t key_hash = XXH3_64bits(&kkk, sizeof(kkk));
                uint64_t res = log_kv_->insert(key, payload, key_hash);
                bool ret = db_->insert(key, res);

                #ifdef USE_PROXY
                check_read_queue();
                #endif

                #ifdef USE_NALI_CACHE
                cache_->insert(key, payload, key_hash);
                #endif

                return ret;
            }

            bool search(const T& key, P* payload, bool epoch = false) {
                #ifdef USE_NALI_CACHE
                const T kkk = key;
                uint64_t key_hash = XXH3_64bits(&kkk, sizeof(kkk));
                bool in_cache = cache_->search(key, payload, key_hash);
                if (in_cache)
                    return true;
                #endif

                uint64_t pmem_addr = 0;
                bool ret = db_->search(key, &pmem_addr);
                if (!ret)
                    return ret;
                int8_t numa_id = (pmem_addr >> 56) & 0xff;
                pmem_addr &= 0x00ffffffffffffff;
                
                #ifdef USE_PROXY
                    if (numa_id == nali::thread_id) {
                        *payload = ((nali::Pair_t<T, P> *)pmem_addr)->value();
                    } else {
                        // send read req to remote numa
                        read_req<P> *req_ = new read_req<P>((char*)pmem_addr, payload);
                        readreq_queue_[random() % thread_ids[numa_id].size()].enqueue(req_);
                        auto time_begin = TIME_NOW;
                        // check is_ready
                        while (!req_->is_ready) {
                            {
                                // check read_req queue
                                read_req<P> *req_ = nullptr;
                                while (readreq_queue_[nali::thread_id].try_dequeue(req_)) {
                                    if (!req_->is_ready) {
                                        *(req_->value) = ((nali::Pair_t<T, P> *)req_->value_ptr)->value();
                                        if (req_->is_ready = true)
                                            delete req_;
                                        else
                                            req_->is_ready = true;
                                    } else {
                                        delete req_; // request thread has do read, cur thread need free the space
                                    }
                                }
                            }
                            // handle timeout
                            auto time_now = TIME_NOW;
                            if (std::chrono::duration_cast<std::chrono::microseconds>(time_now - time_begin).count() > 0.5) {
                                req_->is_ready = true;
                                *payload = ((nali::Pair_t<T, P> *)pmem_addr)->value();
                                return ret;
                            }
                        }
                        delete req_;
                    }
                    check_read_queue();
                #else
                    *payload = ((nali::Pair_t<T, P> *)pmem_addr)->value();
                #endif

                #ifdef USE_NALI_CACHE
                // if not exist in cache, put into cache
                if (in_cache == false) {
                    cache_->insert(key, *payload, key_hash);
                }
                #endif
                return ret;
            }

            bool erase(const T& key, bool epoch = false) {
                const T kkk = key;
                uint64_t key_hash = XXH3_64bits(&kkk, sizeof(kkk));
                log_kv_->erase(key, key_hash);
                bool ret = db_->erase(key);

                #ifdef USE_PROXY
                check_read_queue();
                #endif

                #ifdef USE_NALI_CACHE
                cache_->erase(key, key_hash);
                #endif
                return ret;
            }

            bool update(const T& key, const P& payload, bool epoch = false) {
                const T kkk = key;
                uint64_t key_hash = XXH3_64bits(&kkk, sizeof(kkk));
                uint64_t res = log_kv_->update(key, payload, key_hash);
                bool ret = db_->update(key, res);

                #ifdef USE_PROXY
                check_read_queue();
                #endif

                #ifdef USE_NALI_CACHE
                cache_->update(key, payload, key_hash);
                #endif
                return ret;
            }

            // TODO(zzy) : range scan's proxy!
            // scan not go through cache
            int range_scan_by_size(const T& key, uint32_t to_scan, V* &result = nullptr, bool epoch = false) {
                // std::pair<T, uint64_t> *t_values = new std::pair<T, uint64_t>[to_scan];
                // int scan_size = db_->range_scan_by_size(key, to_scan, t_values, epoch);
                // for (int i = 0; i < scan_size; i++) {
                //     result[i].first = t_values[i].first;
                //     result[i].second = ((nali::Pair_t<T, P> *)t_values[i].second)->value();
                // }

                // #ifdef USE_PROXY
                // check_read_queue();
                // #endif
                // return scan_size;
            }

            void get_depth_info() {
                db_->get_depth_info();
            }

        private:
            #ifdef USE_PROXY
            void check_read_queue() {
                // check read_req queue
                read_req<P> *req_ = nullptr;
                while (readreq_queue_[nali::thread_id].try_dequeue(req_)) {
                    if (!req_->is_ready) {
                        *(req_->value) = ((nali::Pair_t<T, P> *)req_->value_ptr)->value();
                        if (req_->is_ready = true)
                            delete req_;
                        else
                            req_->is_ready = true;
                    } else {
                        delete req_; // request thread has do read, cur thread need free the space
                        req_ = nullptr;
                    }
                }
            }
            #endif

        private:
            Tree<T, uint64_t> *db_; // char* store nvm addr
            nali::LogKV<T, P> *log_kv_;
            std::vector<std::vector<int>> thread_ids; // per_numa_ids

            #ifdef USE_PROXY
            moodycamel::ConcurrentQueue<read_req<P>*> readreq_queue_[max_thread_num];
            #endif

            #ifdef USE_NALI_CACHE
            nali::nali_lrucache<T, P> *cache_;
            #endif
    };
}