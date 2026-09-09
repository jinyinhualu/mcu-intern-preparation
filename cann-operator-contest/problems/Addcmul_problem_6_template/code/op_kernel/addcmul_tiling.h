// Tiling结构体定义的头文件
#pragma once

#include <cstdint>

constexpr uint32_t ADDCMUL_MAX_DIMS = 8;

struct AddcmulTilingData {
    uint64_t totalLength;
    uint64_t inputLength;
    uint64_t x1Length;
    uint64_t x2Length;
    uint32_t rank;
    uint32_t sameShape;
    uint32_t blockDim;
    uint64_t outShape[ADDCMUL_MAX_DIMS];
    uint64_t inputShape[ADDCMUL_MAX_DIMS];
    uint64_t x1Shape[ADDCMUL_MAX_DIMS];
    uint64_t x2Shape[ADDCMUL_MAX_DIMS];
    uint64_t inputStride[ADDCMUL_MAX_DIMS];
    uint64_t x1Stride[ADDCMUL_MAX_DIMS];
    uint64_t x2Stride[ADDCMUL_MAX_DIMS];
};
