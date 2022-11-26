#pragma once

#include "tree.h"
#include "util/utils.h"
#include "../src/nali.h"
#include "../third/alexol/alex.h"
// #include "../third/ALEX/alex.h"
#include "../third/FAST_FAIR/btree.h"

#include "../third/ARTSynchronized/OptimisticLockCoupling/Tree.h"
#include"../third/ARTSynchronized/OptimisticLockCoupling/Tree.cpp"
#include "tbb/tbb.h"
#include <utility>

namespace nali {
    template <class T, class P>
    class nalidb : public Tree<T, P> {
        public:
            typedef std::pair<T, P> V;
            nalidb() {
                db_ = new nali::Nali();
                db_->Init();
            }

            ~nalidb() {
                delete db_;
            }

            void bulk_load(const V values[], int num_keys) {
                // db_->bulk_load(values, num_keys);
                for (int i = 0; i < num_keys; i++) {
                    db_->Put(values[i].first, values[i].second);
                }
            }

            bool insert(const T& key, const P& payload, bool epoch = false) {
                return db_->Put(key, payload);
            }

            bool search(const T& key, P* payload, bool epoch = false) {
                return db_->Get(key, *payload); 
            }

            bool erase(const T& key, bool epoch = false) {
                return db_->Delete(key);
            }

            bool update(const T& key, const P& payload, bool epoch = false) {
                return db_->Update(key, payload);
            }

            int range_scan_by_size(const T& key, uint32_t to_scan, V* &result = nullptr, bool epoch = false) {
                // auto scan_size = db_->Scan(key, static_cast<int>(to_scan), result);
                // return to_scan;
                // TODO
                assert(false);
                return 0;
            }

            void get_info() {
                db_->Info();
            }

        private:
            nali::Nali *db_;
            shared_mutex_u8 rwlock;
    };

    template <class T, class P>
    class alexoldb : public Tree<T, P> {
        public:
            typedef std::pair<T, P> V;
            alexoldb() {
                db_ = new alexol::Alex<T, P>();
                db_->set_max_model_node_size(1 << 24);
                db_->set_max_data_node_size(1 << 18);
            }

            ~alexoldb() {
                delete db_;
            }

            void bulk_load(const V values[], int num_keys) {
                db_->bulk_load(values, num_keys);
            }

            bool insert(const T& key, const P& payload, bool epoch = false) {
                return db_->insert(key, payload);
            }

            bool search(const T& key, P* payload, bool epoch = false) {
                auto ret = db_->get_payload(key, payload);
                return ret;
            }

            bool erase(const T& key, bool epoch = false) {
                int num = db_->erase(key);
                if(num) return true; 
                else return false;
            }

            bool update(const T& key, const P& payload, bool epoch = false) {
                return db_->update(key, payload);
            }

            int range_scan_by_size(const T& key, uint32_t to_scan, V* &result = nullptr, bool epoch = false) {
                auto scan_size = db_->range_scan_by_size(key, static_cast<uint32_t>(to_scan), result);
                return to_scan;
            }

            void get_info() {
                // std::cout << "dram use: " <<  \
                //     (db_->model_size() + db_->data_size()) / 1024.0 / 1024.0 / 1024.0 << "GB" << std::endl;
                auto stat = db_->get_stats();
                std::cout << "num_model_nodes: " << stat.num_model_nodes << "\n"
                        << "num_data_nodes: " << stat.num_data_nodes << "\n"
                        << "num_expand_and_scales: " << stat.num_expand_and_scales << "\n"
                        << "num_expand_and_retrains: " << stat.num_expand_and_retrains << "\n"
                        << "num_downward_splits: " << stat.num_downward_splits << "\n"
                        << "num_sideways_splits: " << stat.num_sideways_splits << "\n"
                        << "num_model_node_expansions: " << stat.num_model_node_expansions << "\n"
                        << "num_model_node_splits: " << stat.num_model_node_splits << "\n";
                // std::cout << "root child nums: " << db_->get_root_children_nums() << "\n";
            }

        private:
            alexol::Alex <T, P, alexol::AlexCompare, std::allocator<std::pair <T, P>>, false> *db_;
    };

    template <class T, class P>
    class fastfairdb : public Tree<T, P> {
        public:
            typedef std::pair<T, P> V;
            fastfairdb() {
                // if (FileExists(pool_name)) {
                //     ::remove(pool_name);
                // }
                // my_alloc::BasePMPool::Initialize(pool_name, pool_size);
                db_ = new fastfair::btree();
            }

            ~fastfairdb() {
                delete db_;
                // my_alloc::BasePMPool::ClosePool();
                // ::remove(pool_name);
            }

            void bulk_load(const V values[], int num_keys) {
                for (int i = 0; i < num_keys; i++) {
                    db_->btree_insert(values[i].first, (char *)values[i].second);
                }
            }

            bool insert(const T& key, const P& payload, bool epoch = false) {
                db_->btree_insert(key, (char *)payload);
                return true;
            }

            bool search(const T& key, P* payload, bool epoch = false) {
                *payload = (P)db_->btree_search(key);
                return true;
            }

            bool erase(const T& key, bool epoch = false){
                db_->btree_delete(key);
                return true;
            }

            bool update(const T& key, const P& payload, bool epoch = false) {
                db_->btree_delete(key);
                db_->btree_insert(key, (char *)payload);
                return true;
            }

            int range_scan_by_size(const T& key, uint32_t to_scan, V* &result = nullptr, bool epoch = false) {
                std::vector<std::pair<fastfair::entry_key_t, uint64_t>> res;
                int scan = to_scan;
                db_->btree_search_range(key, UINT64_MAX, res, scan);
                if (result == nullptr) {
                    result = new V[scan];
                }
                for (int i = 0; i < scan; i++) {
                    result[i].first = res[i].first;
                    result[i].second = res[i].second;
                }
                return scan;
            }

            void get_info() {}

        private:
            fastfair::btree *db_;
    };

    template <class T, class P>
    class artdb : public Tree<T, P> {
        public:
            typedef std::pair<T, P> V;
            artdb() {
                idx = new ART_OLC::Tree(loadKey);
                auto temp= new std::pair<T,P>(~0ull,0);
                loadKey(reinterpret_cast<uint64_t>(&temp), maxKey);
            }

            ~artdb() {
                delete idx;
            }

            void bulk_load(const V values[], int num_keys) {
                for (auto i = 0; i < num_keys; i++) {
                    insert(values[i].first, values[i].second);
                }
            }

            bool insert(const T& key, const P& payload, bool epoch = false) {
                thread_local static auto t = idx->getThreadInfo();

                auto temp = new std::pair<T, P>(key, payload);
                Key k;
                T reversed = swap_endian(key);
                // k.setKeyLen(sizeof(key));
                // reinterpret_cast<KEY_TYPE *>(&k[0])[0] = swap_endian(key);
                k.set(reinterpret_cast<char *>(&reversed), sizeof(key));
                idx->insert(k, reinterpret_cast<TID>(temp),t);
                return true;
            }

            bool search(const T& key, P* payload, bool epoch = false) {
                thread_local static auto t = idx->getThreadInfo();
                Key k;
                T reversed = swap_endian(key);
                // k.setKeyLen(sizeof(key));
                // reinterpret_cast<KEY_TYPE *>(&k[0])[0] = swap_endian(key);
                k.set(reinterpret_cast<char *>(&reversed), sizeof(key));
                auto valPtr = reinterpret_cast<std::pair<T, P>*>(idx->lookup(k,t));
                if(valPtr) {
                    *payload = valPtr->second;
                    return true;
                } else {
                    return false;
                }
            }

            bool erase(const T& key, bool epoch = false) {
                thread_local static auto t = idx->getThreadInfo();
                Key k;
                T reversed = swap_endian(key);
                // k.setKeyLen(sizeof(key));
                // reinterpret_cast<KEY_TYPE *>(&k[0])[0] = swap_endian(key);
                k.set(reinterpret_cast<char *>(&reversed), sizeof(key));
                idx->remove(k, t);
                return true;
            }

            bool update(const T& key, const P& payload, bool epoch = false) {
                thread_local static auto t = idx->getThreadInfo();
                auto temp = new std::pair<T, P>(key,payload);
                Key k;
                T reversed = swap_endian(key);
                // k.setKeyLen(sizeof(key));
                // reinterpret_cast<KEY_TYPE *>(&k[0])[0] = swap_endian(key);
                k.set(reinterpret_cast<char *>(&reversed), sizeof(key));
                return idx->update(k, reinterpret_cast<TID>(temp), t);
            }

            int range_scan_by_size(const T& key, uint32_t to_scan, V* &result = nullptr, bool epoch = false) {
                thread_local static auto t = idx->getThreadInfo();
                Key k;
                k.setKeyLen(sizeof(key));
                reinterpret_cast<T *>(&k[0])[0] = swap_endian(key);

                TID results[to_scan];
                size_t resultCount;
                Key continueKey;
                idx->lookupRange(k, maxKey, continueKey, results, to_scan, resultCount, t);
                // printf("%lu, ", resultCount);

                return resultCount;
            }

            static void loadKey(TID tid, Key &key) {
                // Store the key of the tuple into the key vector
                // Implementation is database specific

                std::pair<T, P> * valPtr = reinterpret_cast<std::pair<T, P> *>(tid);
                T reversed = swap_endian(valPtr->first);
                key.set(reinterpret_cast<char *>(&reversed), sizeof(valPtr->first));
                // key.setKeyLen(sizeof(valPtr->first));
                // reinterpret_cast<KEY_TYPE *>(&key[0])[0] = swap_endian(valPtr->first);
            }

            void get_info() {
            }

        private:
            Key maxKey;
            ART_OLC::Tree *idx;
            inline static uint32_t swap_endian(uint32_t i) {
                return __builtin_bswap32(i);
            }
            inline static uint64_t swap_endian(uint64_t i) {
                return __builtin_bswap64(i);
            }
    };
}

