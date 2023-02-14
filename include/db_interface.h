#pragma once

#include "tree.h"
#include "util/utils.h"
#include "../src/nali.h"
#include "../third/alexol/alex.h"

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

            bool insert(const T& key, const P& payload) {
                return db_->Put(key, payload);
            }

            bool search(const T& key, P &payload) {
                return db_->Get(key, payload); 
            }

            bool erase(const T& key, uint64_t *log_offset = nullptr) {
                return db_->Delete(key, log_offset);
            }

            bool update(const T& key, const P& payload, uint64_t *log_offset = nullptr) {
                return db_->Update(key, payload, log_offset);
            }

            int range_scan_by_size(const T& key, uint32_t to_scan, V* &result = nullptr) {
                int scan_size = to_scan;
                std::vector<std::pair<uint64_t, uint64_t>> res;
                db_->Scan(key, scan_size, res);
                assert(scan_size == res.size());
                for (int i = 0; i < res.size(); i++) {
                    result[i] = res[i];
                }
                return res.size();
            }

            void get_info() {
                db_->Info();
            }

        private:
            nali::Nali *db_;
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

            bool insert(const T& key, const P& payload) {
                return db_->insert(key, payload);
            }

            bool search(const T& key, P &payload) {
                auto ret = db_->get_payload(key, &payload);
                return ret;
            }

            bool erase(const T& key, uint64_t *log_offset = nullptr) {
                int num = db_->erase(key, log_offset);
                if(num) return true; 
                else return false;
            }

            bool update(const T& key, const P& payload, uint64_t *log_offset = nullptr) {
                return db_->update(key, payload, log_offset);
            }

            int range_scan_by_size(const T& key, uint32_t to_scan, V* &result = nullptr) {
                auto scan_size = db_->range_scan_by_size(key, static_cast<uint32_t>(to_scan), result);
                return to_scan;
            }

            void get_info() {
                // // std::cout << "dram use: " <<  \
                // //     (db_->model_size() + db_->data_size()) / 1024.0 / 1024.0 / 1024.0 << "GB" << std::endl;
                // auto stat = db_->get_stats();
                // std::cout << "num_model_nodes: " << stat.num_model_nodes << "\n"
                //         << "num_data_nodes: " << stat.num_data_nodes << "\n"
                //         << "num_expand_and_scales: " << stat.num_expand_and_scales << "\n"
                //         << "num_expand_and_retrains: " << stat.num_expand_and_retrains << "\n"
                //         << "num_downward_splits: " << stat.num_downward_splits << "\n"
                //         << "num_sideways_splits: " << stat.num_sideways_splits << "\n"
                //         << "num_model_node_expansions: " << stat.num_model_node_expansions << "\n"
                //         << "num_model_node_splits: " << stat.num_model_node_splits << "\n";
                // // std::cout << "root child nums: " << db_->get_root_children_nums() << "\n";
            }

        private:
            alexol::Alex <T, P, alexol::AlexCompare, std::allocator<std::pair <T, P>>, false> *db_;
    };
}

