#pragma once

#include <sys/types.h>
#include <dirent.h>
#include <string>

#include "tree.h"
#include "nali_kvlog.h"
#include "util/utils.h"
#include "util/xxhash.h"
#include "util/concurrentqueue.h"
#include "thread_pool.h"
#include "../src/nali_kvlog.h"

// 线程池宏
// #define SCAN_USE_THREAD_POOL
#define PER_THREAD_POOL_THREADS 8

extern thread_local size_t global_thread_id;

namespace nali {

struct logdb_statistic {
    #ifdef USE_PROXY
    size_t remote_read_times;
    #endif
};

#ifdef USE_PROXY
extern thread_local size_t random_thread_id[numa_max_node]; 
#endif

#ifdef USE_PROXY
    template <class P>
    struct read_req {
        char *value_ptr;
        std::atomic<int> user_cnt;
        bool is_ready;
        P *value;
        read_req() : value_ptr(nullptr), is_ready(false), value(nullptr), user_cnt(2) {}
        read_req(char *addr, P *paylaod) : value_ptr(addr), is_ready(false), value(paylaod), user_cnt(2) {}
        void set_req(char *addr, P *paylaod) {
            value_ptr = addr;
            is_ready = false;
            value_ptr = paylaod;
            user_cnt = 2;
        }
    };
#endif

    template <class T, class P>
    static inline void scan_help(std::pair<T, P>* &result, const std::pair<T, uint64_t> *t_values, const std::vector<int> pos_arr, int *complete_elems) {
        for (int pos : pos_arr) {
            result[pos].first = t_values[pos].first;
            nali::Pair_t<T, P> pt;
            pt.load((char*)t_values[pos].second);
            result[pos].second = pt.value();
        }
        ADD(complete_elems, pos_arr.size());
    }

    template <class T, class P>
    class logdb : public Tree<T, P> {
        public:
            typedef std::pair<T, P> V;
            logdb(Tree<T, uint64_t> *db, const std::vector<std::vector<int>> &thread_arr, bool recovery = false, size_t recovery_thread_num = 1) : db_(db) {
                log_kv_ = new nali::LogKV<T, P>(recovery);
                thread_ids = thread_arr;
                for (int i = 0; i < numa_node_num; i++)
                    thread_pool_[i] = new ThreadPool(i, PER_THREAD_POOL_THREADS);

                if (recovery) {
                    log_kv_->recovery_logkv(db, recovery_thread_num);
                }
                // std::atomic<int> gc_thread_id(0);
                // const int gc_thread_num = 8;
                // const int per_thread_part = NALI_VERSION_SHARDS / gc_thread_num;
                // for(int i = 0; i < gc_thread_num; i++) {
                //     gc_threads.emplace_back([&](){
                //         int thread_id = gc_thread_id.fetch_add(1);
                //         int start_pos = thread_id * per_thread_part;
                //         background_gc(this, log_kv_, start_pos, start_pos + per_thread_part);
                //     });
                // }
            }

            ~logdb() {
                // TODO: kill gc threads
                for (auto& t : gc_threads)
                    t.join();
                delete db_;
                delete log_kv_;
            }

            // TODO: 多个NUMA平分keys
            void bulk_load(const V values[], int num_keys) {
                std::pair<T, uint64_t> *dram_kvs = new std::pair<T, uint64_t>[num_keys];
                for (int i = 0; i < num_keys; i++) {
                    const T kkk = values[i].first;
                    uint64_t key_hash = XXH3_64bits(&kkk, sizeof(kkk));
                    uint64_t log_offset = log_kv_->insert(values[i].first, values[i].second, key_hash);
                    dram_kvs[i].first = values[i].first;
                    dram_kvs[i].second = log_offset;
                }
                db_->bulk_load(dram_kvs, num_keys);
                delete [] dram_kvs;
            }

            // dram index kv pair: <key, vlog's vlen+numa_id+file_num+offest>
            bool insert(const T& key, const P& payload) {
                const T kkk = key;
                uint64_t key_hash = XXH3_64bits(&kkk, sizeof(kkk));
                uint64_t log_offset = log_kv_->insert(key, payload, key_hash);
                bool ret = db_->insert(key, log_offset);

                #ifdef USE_PROXY
                check_read_queue();
                #endif

                return ret;
            }

            char *get_value_log_addr(const T&key) {
                const T kkk = key;
                uint64_t key_hash = XXH3_64bits(&kkk, sizeof(kkk));
                uint64_t addr_info = 0;
                bool ret = db_->search(key, &addr_info);
                if (!ret) {
                    std::cout << "wrong key!" << std::endl;
                    exit(1);
                }
                last_log_offest log_info(addr_info);
                return log_kv_->get_addr(log_info, key_hash);
            }

            bool search(const T& key, P &payload) {
                const T kkk = key;
                uint64_t key_hash = XXH3_64bits(&kkk, sizeof(kkk));

                uint64_t addr_info = 0;
                bool ret = db_->search(key, addr_info);
                if (!ret)
                    return ret;
                last_log_offest log_info(addr_info);
                uint64_t pmem_addr = reinterpret_cast<uint64_t>(log_kv_->get_addr(log_info, key_hash));
                
                #ifdef USE_PROXY
                    int8_t numa_id = log_info.numa_id_;
                    if (numa_id == get_numa_id(global_thread_id)) {
                        payload = ((nali::Pair_t<T, P> *)pmem_addr)->value();
                    } else {
                        // send read req to remote numa
                        read_req<P> *req_ = new read_req<P>((char*)pmem_addr, payload);
                        readreq_queue_[random() % thread_ids[numa_id].size()].enqueue(req_);
                        auto time_begin = TIME_NOW;
                        // check is_ready
                        while (!req_->is_ready) {
                            check_read_queue(1);
                            // handle timeout
                            auto time_now = TIME_NOW;
                            if (std::chrono::duration_cast<std::chrono::microseconds>(time_now - time_begin).count() > 0.4) {
                                int cnt_before = 2;
                                int cnt_after = 1;
                                bool res = req_->user_cnt.compare_exchange_strong(cnt_before, cnt_after, std::memory_order_acquire);
                                if (res == false) { 
                                    while (!req_->is_ready) {
                                        check_read_queue(1);
                                    }
                                    delete req_;
                                } else { // do remote read self
                                    payload = ((nali::Pair_t<T, P> *)pmem_addr)->value();
                                }
                                return ret;
                            }
                        }
                    }
                    check_read_queue();
                #else
                    nali::Pair_t<T, P> pt;
                    pt.load((char*)pmem_addr);
                    payload = pt.value();
                    
                    // #ifdef VARVALUE
                    //     // std::string v;
                    //     payload.assign(((char*)pmem_addr) + 28, log_info.vlen_);
                    // #else
                        // payload = ((nali::Pair_t<T, P>*)pmem_addr)->_value;
                    // #endif
                #endif

                return ret;
            }

            bool erase(const T& key, uint64_t *log_offset = nullptr) {
                const T kkk = key;
                uint64_t key_hash = XXH3_64bits(&kkk, sizeof(kkk));

                uint64_t old_log_offset;
                bool ret = db_->erase(key, &old_log_offset);
                log_kv_->erase(key, key_hash, old_log_offset);
                #ifdef USE_PROXY
                check_read_queue();
                #endif

                return ret;
            }

            bool update(const T& key, const P& payload, uint64_t *log_offset = nullptr) {
                const T kkk = key;
                uint64_t key_hash = XXH3_64bits(&kkk, sizeof(kkk));

                Pair_t<T, P> p(key, payload);
                uint64_t cur_log_addr;
                char *log_addr = log_kv_->update_step1(key, payload, key_hash, p, cur_log_addr);

                #ifdef USE_PROXY
                check_read_queue();
                #endif

                uint64_t _log_offset;
                bool ret = db_->update(key, cur_log_addr, &_log_offset);
                if (!ret)
                    return ret;

                // update_step2
                {
                    p.set_last_log_offest(_log_offset);
                    p.store_persist(log_addr);
                }

                #ifdef USE_PROXY
                check_read_queue();
                #endif

                return ret;
            }

            // scan not go through cache
            int range_scan_by_size(const T& key, uint32_t to_scan, V* &result = nullptr) {
                // 1. get relative addrs
                std::pair<T, uint64_t> *t_values = new std::pair<T, uint64_t>[to_scan];
                int scan_size = db_->range_scan_by_size(key, to_scan, t_values);
                int complete_reads = 0;
                std::vector<std::vector<int>> numa_pos_arr(numa_node_num);
                // 2. trans realtive addrs to absolute addrs
                for (int i = 0; i < scan_size; i++) {
                    T kkk = t_values[i].first;
                    uint64_t key_hash = XXH3_64bits(&kkk, sizeof(kkk));
                    last_log_offest log_info(t_values[i].second);
                    t_values[i].second = reinterpret_cast<uint64_t>(log_kv_->get_addr(log_info, key_hash));
                    
                #ifdef SCAN_USE_THREAD_POOL
                    {
                        numa_pos_arr[log_info.numa_id_].push_back(i);
                        // batch send to background thread,不同numa vlog发送到对应线程池处理
                        // if (numa_pos_arr[log_info.numa_id_].size() % 45 == 0) {
                        //     thread_pool_[log_info.numa_id_]->execute(scan_help<T, P>, result, t_values, numa_pos_arr[log_info.numa_id_], &complete_reads);
                        //     numa_pos_arr[log_info.numa_id_].clear();
                        // }
                        if (unlikely(i == scan_size-1)) {
                            for (int j = 0; j < numa_node_num; j++) {
                                if (numa_pos_arr[j].size() != 0) {
                                    thread_pool_[j]->execute(scan_help<T,P>, result, t_values, numa_pos_arr[j], &complete_reads);
                                }
                            }
                            std::this_thread::yield();
                            // wait until work is complete
                            while (LOAD(&complete_reads) != scan_size) {
                                std::this_thread::yield();
                            }
                        }
                    }
                #endif
                }

                #ifndef SCAN_USE_THREAD_POOL
                // or do work itself
                for (int i = 0; i < scan_size; i++) {
                    result[i].first = t_values[i].first;
                    nali::Pair_t<T, P> pt;
                    pt.load((char*)t_values[i].second);
                    result[i].second = pt.value();
                }
                #endif

                #ifdef USE_PROXY
                check_read_queue();
                #endif
                return scan_size;
            }

            void get_info() {
                db_->get_info();
            }

        private:
            #ifdef USE_PROXY
            void check_read_queue(int loop_times = 3) {
                // check read_req queue
                read_req<P> *req_ = nullptr;
                while (loop_times-- && readreq_queue_[global_thread_id].try_dequeue(req_)) {
                    int cnt_before = 2;
                    int cnt_after = 1;
                    bool res = req_->user_cnt.compare_exchange_strong(cnt_before, cnt_after, std::memory_order_acquire);
                    if (res == false) { // request is timeout
                        delete req_;
                    } else {
                        *(req_->value) = ((nali::Pair_t<T, P> *)(req_->value_ptr))->value();
                        req_->is_ready = true;
                    }
                }
            }
            #endif

            // 目前只支持定长32B log
            void background_gc(logdb *db, nali::LogKV<T, P> *log_kv_, int begin_pos, int end_pos) {
                sleep(100);
                while (true) {
                    // 1. wait until can do gc(TODO)
                    sleep(20);
                    // 2. 获取当前最新日志文件号l，从日志号l-1文件开始回收
                        // 对于put/update log，回到dram index get(key, &value)判断log是否valid
                            // 如果valid，则把其后边的日志全部置为无效，再次执行一次该log后删除该log
                            // 如果invalid，则回溯删除所有old log和本log
                        // 对于delete log，回溯删除掉所有old log和本log
                    for (int i = MAX_LOF_FILE_ID; i >= 0; i--) {
                        for (int j = begin_pos; j < end_pos; j++) {
                            for (int k = 0; k < numa_node_num; k++) {
                                char *start_addr = log_kv_->get_page_start_addr(k, j, i);
                                for (int t = 0; t < PPAGE_SIZE; t += 32) { // TODO:var value log
                                    Pair_t<T, P> *log_ = (Pair_t<T, P> *)(start_addr+t);
                                    OP_t log_op_ = log_->get_op();
                                    if (log_op_ == OP_t::INSERT || log_op_ == OP_t::UPDATE || log_op_ == OP_t::DELETE) {
                                        std::vector<Pair_t<T, P> *> old_log;
                                        const T kkk = log_->key();
                                        uint64_t key_hash = XXH3_64bits(&kkk, sizeof(kkk));
                                        if (log_op_ == OP_t::DELETE) {
                                            old_log.push_back(log_);
                                        } else {
                                            char *dram_vlog_addr = db->get_value_log_addr(log_->key());
                                            if (dram_vlog_addr != (char *)log_) {
                                                old_log.push_back(log_);
                                            }
                                        }
                                        
                                        uint64_t next_old_log = log_->next_old_log();
                                        while (next_old_log != UINT64_MAX) { // 最old的日志，next log地址为全1
                                            last_log_offest log_info(next_old_log);
                                            log_ = (nali::Pair_t<T, P> *)log_kv_->get_addr(log_info, key_hash);
                                            if (log_->get_op() == OP_t::TRASH)
                                                break;
                                            old_log.push_back(log_);
                                            next_old_log = log_->next_old_log();
                                        }
                                        for (int p = old_log.size(); p >= 0; p--) {
                                            log_ = old_log[p];
                                            log_->set_op_persist(OP_t::TRASH);
                                        }
                                    } else if (log_op_ == OP_t::TRASH) {
                                        continue;
                                    } else {
                                        std::cout << "gc_thread: invalid log op" << std::endl;
                                        exit(1);
                                    }
                                }
                            }
                        }
                    }
                }
            }

        private:
            Tree<T, uint64_t> *db_;
            nali::LogKV<T, P> *log_kv_;
            std::vector<std::vector<int>> thread_ids; // per_numa_ids
            ThreadPool *thread_pool_[numa_node_num];
            std::vector<std::thread> gc_threads;
            #ifdef USE_PROXY
            moodycamel::ConcurrentQueue<read_req<P>*> readreq_queue_[max_thread_num];
            #endif
    };
}