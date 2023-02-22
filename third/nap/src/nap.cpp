#include "nap.h"
#include "nap_wrapper.h"
char nap_max_str[15] = {(int8_t)255, (int8_t)255, (int8_t)255, (int8_t)255,
                    (int8_t)255,
                    (int8_t)255, (int8_t)255, (int8_t)255, (int8_t)255,
                    (int8_t)255,
                    (int8_t)255, (int8_t)255, (int8_t)255, (int8_t)255,
                    (int8_t)255};
namespace nap {

pmem::obj::pool_base nap_pop_numa[kMaxNumaCnt];
ThreadMeta thread_meta_array[kMaxThreadCnt];

CowAlloctor *cow_alloc;

} // namespace nap
