#pragma once

#include <stdint.h>
#include <stddef.h>

uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len);
uint32_t crc32_bytes(const uint8_t* data, size_t len);
