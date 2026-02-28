#pragma once

#include <stdlib.h>

inline bool parseIntStrict(const char* s, int min_v, int max_v, int& out) {
    if (s == nullptr || *s == '\0') {
        return false;
    }

    char* end = nullptr;
    long value = strtol(s, &end, 10);
    if (end == s || *end != '\0') {
        return false;
    }
    if (value < min_v || value > max_v) {
        return false;
    }

    out = static_cast<int>(value);
    return true;
}
