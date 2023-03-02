#ifndef ART_KEY_H
#define ART_KEY_H

#include <stdint.h>
#include <cstring>
#include <memory>
#include <assert.h>

extern std::atomic<uint64_t> dram_allocated;
extern std::atomic<uint64_t> pmem_allocated;
extern std::atomic<uint64_t> dram_freed;
extern std::atomic<uint64_t> pmem_freed;

using KeyLen = uint32_t;

class Key {
public:

    static constexpr uint32_t stackLen = 128;
    uint32_t len = 0;

    uint8_t *data;

    uint8_t stackKey[stackLen];

    Key(uint64_t k) { setInt(k); }

    void setInt(uint64_t k) {
	    data = stackKey; 
	    len = 8; 
	    *reinterpret_cast<uint64_t*>(stackKey) = __builtin_bswap64(k); 
    }

    Key() {}

    ~Key();

    Key(const Key &key) = delete;

    Key(Key &&key);

    void set(const char bytes[], const std::size_t length);

    void operator=(const char key[]);

    bool operator==(const Key &k) const {
        if (k.getKeyLen() != getKeyLen()) {
            return false;
        }
        return std::memcmp(&k[0], data, getKeyLen()) == 0;
    }

    bool operator<(const Key &k) const {
            for (uint32_t i = 0; i < std::min(k.getKeyLen(), getKeyLen()); ++i) {
                    if (data[i] > k[i])
                            return false;
                    else if (data[i] < k[i])
                            return true;
            }
    }

    uint8_t &operator[](std::size_t i);

    const uint8_t &operator[](std::size_t i) const;

    KeyLen getKeyLen() const;

    void setKeyLen(KeyLen len);

};


inline uint8_t &Key::operator[](std::size_t i) {
    assert(i < len);
    return data[i];
}

inline const uint8_t &Key::operator[](std::size_t i) const {
    assert(i < len);
    return data[i];
}

inline KeyLen Key::getKeyLen() const { return len; }

inline Key::~Key() {
    if (len > stackLen) {
        delete[] data;
        data = nullptr;
    }
}

inline Key::Key(Key &&key) {
    len = key.len;
    if (len > stackLen) {
        data = key.data;
        key.data = nullptr;
    } else {
        memcpy(stackKey, key.stackKey, key.len);
        data = stackKey;
    }
}

inline void Key::set(const char bytes[], const std::size_t length) {
    if (len > stackLen) {
        delete[] data;
    }
    if (length <= stackLen) {
        memcpy(stackKey, bytes, length);
        data = stackKey;
    } else {
        data = new uint8_t[length];
    #ifdef MEMORY_FOOTPRINT
        dram_allocated += length;
    #endif
        memcpy(data, bytes, length);
    }
    len = length;
}

inline void Key::operator=(const char key[]) {
    if (len > stackLen) {
        delete[] data;
    }
    len = strlen(key);
    if (len <= stackLen) {
        memcpy(stackKey, key, len);
        data = stackKey;
    } else {
        data = new uint8_t[len];
    #ifdef MEMORY_FOOTPRINT
        dram_allocated += len;
    #endif
        memcpy(data, key, len);
    }
}

inline void Key::setKeyLen(KeyLen newLen) {
    if (len == newLen) return;
    if (len > stackLen) {
        delete[] data;
    }
    len = newLen;
    if (len > stackLen) {
        data = new uint8_t[len];
    #ifdef MEMORY_FOOTPRINT
        dram_allocated += len;
    #endif
    } else {
        data = stackKey;
    }
}

#endif // ART_KEY_H
