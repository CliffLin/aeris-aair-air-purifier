#pragma once

#include "Particle.h"
#include "string_safety.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

inline bool isDeviceIdTopicSafe(const char* s, size_t max_len = 31) {
    if (s == nullptr) {
        return false;
    }

    size_t len = strlen(s);
    if (len == 0 || len > max_len) {
        return false;
    }

    for (size_t i = 0; i < len; ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (!(isalnum(c) || c == '_' || c == '-')) {
            return false;
        }
    }
    return true;
}

inline bool isTopicRootSafe(const char* s, size_t max_len = 64) {
    if (s == nullptr) {
        return false;
    }

    size_t len = strlen(s);
    if (len == 0 || len >= max_len) {
        return false;
    }
    if (s[0] == '/' || s[len - 1] == '/') {
        return false;
    }

    bool prev_was_slash = false;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == '/') {
            if (prev_was_slash) {
                return false;
            }
            prev_was_slash = true;
            continue;
        }

        prev_was_slash = false;
        if (!(isalnum(c) || c == '_' || c == '-')) {
            return false;
        }
    }

    return true;
}

inline void normalizeDeviceId(const char* src, char* dst, size_t dst_size) {
    if (dst == nullptr || dst_size == 0) {
        return;
    }

    size_t written = 0;
    if (src != nullptr) {
        size_t len = strlen(src);
        for (size_t i = 0; i < len && written + 1 < dst_size; ++i) {
            unsigned char c = static_cast<unsigned char>(src[i]);
            if (isalnum(c) || c == '_' || c == '-') {
                dst[written++] = static_cast<char>(c);
            }
        }
    }

    if (written == 0) {
        String raw = System.deviceID();
        for (size_t i = 0; i < raw.length() && written + 1 < dst_size; ++i) {
            unsigned char c = static_cast<unsigned char>(raw.charAt(i));
            if (isalnum(c) || c == '_' || c == '-') {
                dst[written++] = static_cast<char>(c);
            }
        }
    }

    if (written == 0) {
        safeCopy(dst, dst_size, "device");
        return;
    }

    dst[written] = '\0';
}

inline void buildDefaultTopicRoot(const char* device_id, char* out, size_t out_size) {
    if (out == nullptr || out_size == 0) {
        return;
    }

    char safe_id[32] = {0};
    if (isDeviceIdTopicSafe(device_id, sizeof(safe_id) - 1)) {
        safeCopy(safe_id, sizeof(safe_id), device_id);
    } else {
        normalizeDeviceId(device_id, safe_id, sizeof(safe_id));
    }

    snprintf(out, out_size, "aeris/v2/%s", safe_id);
    out[out_size - 1] = '\0';
}
