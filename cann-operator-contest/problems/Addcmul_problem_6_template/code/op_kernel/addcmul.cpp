// Kernel侧核函数实现
#include "kernel_operator.h"

#include <cstdint>
#include <type_traits>

#include "addcmul_tiling.h"
#include "tiling_key_addcmul.h"

using namespace AscendC;

constexpr uint32_t BUFFER_NUM = 1;
constexpr uint32_t TILE_LENGTH = 1024;

template <class T>
class KernelAddcmul {
public:
    __aicore__ inline KernelAddcmul() {}

    __aicore__ inline void Init(GM_ADDR input_data, GM_ADDR x1, GM_ADDR x2, GM_ADDR value, GM_ADDR y,
                                const AddcmulTilingData &tilingData) {
        totalLength = tilingData.totalLength;
        inputLength = tilingData.inputLength;
        x1Length = tilingData.x1Length;
        x2Length = tilingData.x2Length;
        rank = tilingData.rank;
        sameShape = tilingData.sameShape;
        blockDim = tilingData.blockDim == 0 ? 1 : tilingData.blockDim;

        for (uint32_t i = 0; i < ADDCMUL_MAX_DIMS; ++i) {
            outShape[i] = tilingData.outShape[i];
            inputShape[i] = tilingData.inputShape[i];
            x1Shape[i] = tilingData.x1Shape[i];
            x2Shape[i] = tilingData.x2Shape[i];
            inputStride[i] = tilingData.inputStride[i];
            x1Stride[i] = tilingData.x1Stride[i];
            x2Stride[i] = tilingData.x2Stride[i];
        }

        inputGm.SetGlobalBuffer((__gm__ T *)input_data, inputLength);
        x1Gm.SetGlobalBuffer((__gm__ T *)x1, x1Length);
        x2Gm.SetGlobalBuffer((__gm__ T *)x2, x2Length);
        valueGm.SetGlobalBuffer((__gm__ T *)value, 1);
        yGm.SetGlobalBuffer((__gm__ T *)y, totalLength);
        valueScalar = valueGm.GetValue(0);

        pipe.InitBuffer(inputQueue, BUFFER_NUM, TILE_LENGTH * sizeof(T));
        pipe.InitBuffer(x1Queue, BUFFER_NUM, TILE_LENGTH * sizeof(T));
        pipe.InitBuffer(x2Queue, BUFFER_NUM, TILE_LENGTH * sizeof(T));
        pipe.InitBuffer(yQueue, BUFFER_NUM, TILE_LENGTH * sizeof(T));

        if constexpr (std::is_same_v<T, half>) {
            pipe.InitBuffer(inputFloatBuf, TILE_LENGTH * sizeof(float));
            pipe.InitBuffer(x1FloatBuf, TILE_LENGTH * sizeof(float));
            pipe.InitBuffer(x2FloatBuf, TILE_LENGTH * sizeof(float));
            pipe.InitBuffer(tmpFloatBuf, TILE_LENGTH * sizeof(float));
        } else if constexpr (std::is_same_v<T, float>) {
            pipe.InitBuffer(tmpBuf0, TILE_LENGTH * sizeof(T));
            pipe.InitBuffer(tmpBuf1, TILE_LENGTH * sizeof(T));
        }
    }

    __aicore__ inline void Process() {
        if (totalLength == 0) {
            return;
        }

        SplitCoreRange();
        if (coreLength == 0) {
            return;
        }

        if constexpr (std::is_same_v<T, half> || std::is_same_v<T, float>) {
            if (sameShape != 0) {
                ProcessVectorSameShape();
            } else {
                ProcessScalar();
            }
        } else {
            ProcessScalar();
        }
    }

private:
    __aicore__ inline void SplitCoreRange() {
        const uint32_t blockIdx = GetBlockIdx();
        const uint64_t baseLen = totalLength / blockDim;
        const uint64_t tail = totalLength % blockDim;

        if (blockIdx < tail) {
            coreLength = baseLen + 1;
            coreStart = static_cast<uint64_t>(blockIdx) * coreLength;
        } else {
            coreLength = baseLen;
            coreStart = tail * (baseLen + 1) + (static_cast<uint64_t>(blockIdx) - tail) * baseLen;
        }
    }

    __aicore__ inline void ProcessVectorSameShape() {
        uint64_t processed = 0;
        while (processed < coreLength) {
            const uint32_t curLen = static_cast<uint32_t>(
                (coreLength - processed) > TILE_LENGTH ? TILE_LENGTH : (coreLength - processed));
            const uint64_t gmOffset = coreStart + processed;

            LocalTensor<T> inputLocal = inputQueue.AllocTensor<T>();
            LocalTensor<T> x1Local = x1Queue.AllocTensor<T>();
            LocalTensor<T> x2Local = x2Queue.AllocTensor<T>();
            LocalTensor<T> yLocal = yQueue.AllocTensor<T>();

            DataCopyExtParams copyParams{1, static_cast<uint32_t>(curLen * sizeof(T)), 0, 0, 0};
            DataCopyPadExtParams<T> padParams{false, 0, 0, 0};
            DataCopyPad(inputLocal, inputGm[gmOffset], copyParams, padParams);
            DataCopyPad(x1Local, x1Gm[gmOffset], copyParams, padParams);
            DataCopyPad(x2Local, x2Gm[gmOffset], copyParams, padParams);

            inputQueue.EnQue(inputLocal);
            x1Queue.EnQue(x1Local);
            x2Queue.EnQue(x2Local);

            inputLocal = inputQueue.DeQue<T>();
            x1Local = x1Queue.DeQue<T>();
            x2Local = x2Queue.DeQue<T>();

            CalcVector(inputLocal, x1Local, x2Local, yLocal, curLen);

            yQueue.EnQue(yLocal);
            yLocal = yQueue.DeQue<T>();
            DataCopyPad(yGm[gmOffset], yLocal, copyParams);

            inputQueue.FreeTensor(inputLocal);
            x1Queue.FreeTensor(x1Local);
            x2Queue.FreeTensor(x2Local);
            yQueue.FreeTensor(yLocal);
            processed += curLen;
        }
    }

    __aicore__ inline void CalcVector(LocalTensor<T> inputLocal, LocalTensor<T> x1Local,
                                      LocalTensor<T> x2Local, LocalTensor<T> yLocal, uint32_t count) {
        if constexpr (std::is_same_v<T, half>) {
            LocalTensor<float> inputFloat = inputFloatBuf.Get<float>();
            LocalTensor<float> x1Float = x1FloatBuf.Get<float>();
            LocalTensor<float> x2Float = x2FloatBuf.Get<float>();
            LocalTensor<float> tmpFloat = tmpFloatBuf.Get<float>();

            Cast(inputFloat, inputLocal, RoundMode::CAST_NONE, count);
            Cast(x1Float, x1Local, RoundMode::CAST_NONE, count);
            Cast(x2Float, x2Local, RoundMode::CAST_NONE, count);
            Mul(tmpFloat, x1Float, x2Float, count);
            Muls(x1Float, tmpFloat, static_cast<float>(valueScalar), count);
            Add(x2Float, inputFloat, x1Float, count);
            Cast(yLocal, x2Float, RoundMode::CAST_NONE, count);
        } else {
            LocalTensor<T> tmp0 = tmpBuf0.Get<T>();
            LocalTensor<T> tmp1 = tmpBuf1.Get<T>();
            Mul(tmp0, x1Local, x2Local, count);
            Muls(tmp1, tmp0, valueScalar, count);
            Add(yLocal, inputLocal, tmp1, count);
        }
    }

    __aicore__ inline void ProcessScalar() {
        for (uint64_t outIndex = coreStart; outIndex < coreStart + coreLength; ++outIndex) {
            const uint64_t inputOffset = CalcBroadcastOffset(outIndex, inputShape, inputStride);
            const uint64_t x1Offset = CalcBroadcastOffset(outIndex, x1Shape, x1Stride);
            const uint64_t x2Offset = CalcBroadcastOffset(outIndex, x2Shape, x2Stride);

            const T inputValue = inputGm.GetValue(inputOffset);
            const T x1Value = x1Gm.GetValue(x1Offset);
            const T x2Value = x2Gm.GetValue(x2Offset);
            yGm.SetValue(outIndex, CalcScalar(inputValue, x1Value, x2Value));
        }
    }

    __aicore__ inline uint64_t CalcBroadcastOffset(uint64_t outIndex, const uint64_t shape[ADDCMUL_MAX_DIMS],
                                                   const uint64_t stride[ADDCMUL_MAX_DIMS]) const {
        uint64_t offset = 0;
        uint64_t remain = outIndex;

        for (int32_t dim = static_cast<int32_t>(rank) - 1; dim >= 0; --dim) {
            uint64_t coord = 0;
            if (outShape[dim] != 0) {
                coord = remain % outShape[dim];
                remain = remain / outShape[dim];
            }
            if (shape[dim] != 1) {
                offset += coord * stride[dim];
            }
        }
        return offset;
    }

    __aicore__ inline T CalcScalar(T inputValue, T x1Value, T x2Value) const {
        if constexpr (std::is_same_v<T, half>) {
            const float result = static_cast<float>(inputValue) +
                                 static_cast<float>(x1Value) * static_cast<float>(x2Value) *
                                     static_cast<float>(valueScalar);
            return static_cast<half>(result);
        } else if constexpr (std::is_same_v<T, float>) {
            return inputValue + x1Value * x2Value * valueScalar;
        } else if constexpr (std::is_same_v<T, int8_t>) {
            const int32_t result = static_cast<int32_t>(inputValue) +
                                   static_cast<int32_t>(x1Value) * static_cast<int32_t>(x2Value) *
                                       static_cast<int32_t>(valueScalar);
            return static_cast<int8_t>(result);
        } else {
            const int64_t result = static_cast<int64_t>(inputValue) +
                                   static_cast<int64_t>(x1Value) * static_cast<int64_t>(x2Value) *
                                       static_cast<int64_t>(valueScalar);
            return static_cast<int32_t>(result);
        }
    }

private:
    GlobalTensor<T> inputGm;
    GlobalTensor<T> x1Gm;
    GlobalTensor<T> x2Gm;
    GlobalTensor<T> valueGm;
    GlobalTensor<T> yGm;

    TPipe pipe;
    TQue<QuePosition::VECIN, BUFFER_NUM> inputQueue;
    TQue<QuePosition::VECIN, BUFFER_NUM> x1Queue;
    TQue<QuePosition::VECIN, BUFFER_NUM> x2Queue;
    TQue<QuePosition::VECOUT, BUFFER_NUM> yQueue;
    TBuf<QuePosition::VECCALC> inputFloatBuf;
    TBuf<QuePosition::VECCALC> x1FloatBuf;
    TBuf<QuePosition::VECCALC> x2FloatBuf;
    TBuf<QuePosition::VECCALC> tmpFloatBuf;
    TBuf<QuePosition::VECCALC> tmpBuf0;
    TBuf<QuePosition::VECCALC> tmpBuf1;

    T valueScalar;
    uint64_t totalLength = 0;
    uint64_t inputLength = 0;
    uint64_t x1Length = 0;
    uint64_t x2Length = 0;
    uint64_t coreStart = 0;
    uint64_t coreLength = 0;
    uint32_t rank = 0;
    uint32_t sameShape = 0;
    uint32_t blockDim = 1;
    uint64_t outShape[ADDCMUL_MAX_DIMS] = {0};
    uint64_t inputShape[ADDCMUL_MAX_DIMS] = {0};
    uint64_t x1Shape[ADDCMUL_MAX_DIMS] = {0};
    uint64_t x2Shape[ADDCMUL_MAX_DIMS] = {0};
    uint64_t inputStride[ADDCMUL_MAX_DIMS] = {0};
    uint64_t x1Stride[ADDCMUL_MAX_DIMS] = {0};
    uint64_t x2Stride[ADDCMUL_MAX_DIMS] = {0};
};

template <typename DT_INPUT_DATA>
__global__ __aicore__ void addcmul(GM_ADDR input_data, GM_ADDR x1, GM_ADDR x2, GM_ADDR value, GM_ADDR y,
                                   GM_ADDR workspace, GM_ADDR tiling) {
    REGISTER_TILING_DEFAULT(AddcmulTilingData);
    GET_TILING_DATA_WITH_STRUCT(AddcmulTilingData, tiling_data, tiling);
    KernelAddcmul<DT_INPUT_DATA> op;
    op.Init(input_data, x1, x2, value, y, tiling_data);
    op.Process();
}
