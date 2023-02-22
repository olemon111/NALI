#ifndef ART_KEY_H
#define ART_KEY_H

#include <stdint.h>
#include <cstring>
#include <memory>
#include <assert.h>

using KeyLen = uint32_t;

class DPKey {
public:

    static constexpr uint32_t stackLen = 128;
    uint32_t len = 0;

    uint8_t *data = nullptr;

    uint8_t stackKey[stackLen];

    DPKey(uint64_t k) { setInt(k); }

    void setInt(uint64_t k) { data = stackKey; len = 8; *reinterpret_cast<uint64_t*>(stackKey) = __builtin_bswap64(k); }

    uint64_t getInt() { return __builtin_bswap64(*reinterpret_cast<uint64_t*>(stackKey)); }

    DPKey() {}

    ~DPKey();

    //DPKey(const DPKey &key);

    DPKey(DPKey &&key);

    void set(const char bytes[], const std::size_t length);

    void operator=(const char key[]);
    //DPKey& operator=(const DPKey &key);
    bool operator==(const DPKey &k) const {
        if (k.getKeyLen() != getKeyLen()) {
            return false;
        }
        return std::memcmp(&k[0], data, getKeyLen()) == 0;
    }

    uint8_t &operator[](std::size_t i);

    const uint8_t &operator[](std::size_t i) const;

    KeyLen getKeyLen() const;

    void setKeyLen(KeyLen len);

};


inline uint8_t &DPKey::operator[](std::size_t i) {
    assert(i < len);
    return data[i];
}

inline const uint8_t &DPKey::operator[](std::size_t i) const {
    assert(i < len);
    return data[i];
}

inline KeyLen DPKey::getKeyLen() const { return len; }

inline DPKey::~DPKey() {
    if (len > stackLen) {
        delete[] data;
        data = nullptr;
    }
}

inline DPKey::DPKey(DPKey &&key) {
    len = key.len;
    if (len > stackLen) {
        data = key.data;
        key.data = nullptr;
    } else {
        memcpy(stackKey, key.stackKey, key.len);
        data = stackKey;
    }
}
//
//inline DPKey::DPKey(const DPKey &key) {
//    len = key.len;
//    if (len > stackLen) {
//        data = new uint8_t[len];
//        memcpy(data, key.data, len);
//    } else {
//        memcpy(stackKey, key.stackKey, key.len);
//        data = stackKey;
//    }
//}

inline void DPKey::set(const char bytes[], const std::size_t length) {
    if (len > stackLen) {
        delete[] data;
    }
    if (length <= stackLen) {
        memcpy(stackKey, bytes, length);
        data = stackKey;
    } else {
        data = new uint8_t[length];
        memcpy(data, bytes, length);
    }
    len = length;
}
//
//inline DPKey& DPKey::operator=(const DPKey &key) {
//    if (this == &key)
//        return *this;
//    if (len > stackLen) {
//        delete[] data;
//    }
//
//    data = nullptr;
//    len = key.len;
//
//    if (len > stackLen) {
//        data = new uint8_t[len];
//        memcpy(data, key.data, len);
//    } else {
//        memcpy(stackKey, key.stackKey, key.len);
//        data = stackKey;
//    }
//    return *this;
//}


inline void DPKey::operator=(const char key[]) {
    if (len > stackLen) {
        delete[] data;
    }
    len = strlen(key);
    if (len <= stackLen) {
        memcpy(stackKey, key, len);
        data = stackKey;
    } else {
        data = new uint8_t[len];
        memcpy(data, key, len);
    }
}

inline void DPKey::setKeyLen(KeyLen newLen) {
    if (len == newLen) return;
    if (len > stackLen) {
        delete[] data;
    }
    len = newLen;
    if (len > stackLen) {
        data = new uint8_t[len];
    } else {
        data = stackKey;
    }
}

#endif // ART_KEY_H
