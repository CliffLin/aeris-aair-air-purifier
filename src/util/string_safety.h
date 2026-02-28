#pragma once

#include <stddef.h>
#include <string.h>

inline bool hasNullTerminator(const char* s, size_t len) {
    if (s == nullptr) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        if (s[i] == '\0') {
            return true;
        }
    }
    return false;
}

inline void safeCopy(char* dst, size_t dst_size, const char* src) {
    if (dst == nullptr || dst_size == 0) {
        return;
    }
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}
