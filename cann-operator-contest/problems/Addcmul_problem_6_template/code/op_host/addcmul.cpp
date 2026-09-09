// Host侧Tiling实现
#include "register/op_def_registry.h"

#include "tiling/platform/platform_ascendc.h"

#include "../op_kernel/addcmul_tiling.h"
#include "../op_kernel/tiling_key_addcmul.h"

namespace {
constexpr uint32_t MIN_ELEMS_PER_CORE = 256;

uint64_t MaxU64(uint64_t lhs, uint64_t rhs) {
    return lhs > rhs ? lhs : rhs;
}

uint64_t MinU64(uint64_t lhs, uint64_t rhs) {
    return lhs < rhs ? lhs : rhs;
}

bool CalcBroadcastShape(const gert::Shape *inputShape, const gert::Shape *x1Shape,
                        const gert::Shape *x2Shape, uint64_t outShape[ADDCMUL_MAX_DIMS],
                        uint32_t &rank) {
    const gert::Shape *shapes[3] = {inputShape, x1Shape, x2Shape};
    rank = 0;
    for (uint32_t i = 0; i < 3; ++i) {
        rank = static_cast<uint32_t>(MaxU64(rank, shapes[i]->GetDimNum()));
    }
    if (rank > ADDCMUL_MAX_DIMS) {
        return false;
    }

    for (uint32_t outDim = 0; outDim < rank; ++outDim) {
        uint64_t dim = 1;
        for (uint32_t i = 0; i < 3; ++i) {
            const uint32_t inRank = static_cast<uint32_t>(shapes[i]->GetDimNum());
            uint64_t curDim = 1;
            if (outDim + inRank >= rank) {
                curDim = static_cast<uint64_t>(shapes[i]->GetDim(outDim + inRank - rank));
            }

            if (curDim == 1) {
                continue;
            }
            if (dim == 1) {
                dim = curDim;
                continue;
            }
            if (curDim != dim) {
                return false;
            }
        }
        outShape[outDim] = dim;
    }
    return true;
}

uint64_t CalcShapeSize(const uint64_t shape[ADDCMUL_MAX_DIMS], uint32_t rank) {
    uint64_t total = 1;
    for (uint32_t i = 0; i < rank; ++i) {
        total *= shape[i];
    }
    return total;
}

void FillAlignedShapeAndStride(const gert::Shape *shape, uint32_t rank,
                               uint64_t alignedShape[ADDCMUL_MAX_DIMS],
                               uint64_t stride[ADDCMUL_MAX_DIMS]) {
    const uint32_t inRank = static_cast<uint32_t>(shape->GetDimNum());
    const uint32_t leadingOnes = rank - inRank;

    for (uint32_t i = 0; i < ADDCMUL_MAX_DIMS; ++i) {
        alignedShape[i] = 1;
        stride[i] = 0;
    }

    for (uint32_t i = 0; i < rank; ++i) {
        if (i < leadingOnes) {
            alignedShape[i] = 1;
        } else {
            alignedShape[i] = static_cast<uint64_t>(shape->GetDim(i - leadingOnes));
        }
    }

    uint64_t curStride = 1;
    for (int32_t i = static_cast<int32_t>(rank) - 1; i >= 0; --i) {
        stride[i] = curStride;
        curStride *= alignedShape[i];
    }
}

bool ShapeSizeEquals(const gert::Shape *shape, uint64_t totalLength) {
    return static_cast<uint64_t>(shape->GetShapeSize()) == totalLength;
}
}  // namespace

namespace optiling {
static ge::graphStatus TilingFunc(gert::TilingContext *context) {
    auto platform = platform_ascendc::PlatformAscendC(context->GetPlatformInfo());
    const uint32_t numCoresAiv = static_cast<uint32_t>(platform.GetCoreNumAiv());

    const gert::StorageShape *storageInput = context->GetInputShape(0);
    const gert::StorageShape *storageX1 = context->GetInputShape(1);
    const gert::StorageShape *storageX2 = context->GetInputShape(2);
    const gert::StorageShape *storageValue = context->GetInputShape(3);

    const gert::Shape &inputShape = storageInput->GetStorageShape();
    const gert::Shape &x1Shape = storageX1->GetStorageShape();
    const gert::Shape &x2Shape = storageX2->GetStorageShape();
    const gert::Shape &valueShape = storageValue->GetStorageShape();

    if (valueShape.GetShapeSize() != 1) {
        return ge::GRAPH_FAILED;
    }

    uint64_t outShape[ADDCMUL_MAX_DIMS] = {0};
    uint32_t rank = 0;
    if (!CalcBroadcastShape(&inputShape, &x1Shape, &x2Shape, outShape, rank)) {
        return ge::GRAPH_FAILED;
    }

    const uint64_t totalLength = CalcShapeSize(outShape, rank);

    const gert::Tensor *tensorInputData = context->GetRequiredInputTensor(0);
    ge::DataType dtypeInputData = tensorInputData->GetDataType();
    uint32_t DT_INPUT_DATA = static_cast<uint32_t>(dtypeInputData);
    ASCENDC_TPL_SEL_PARAM(context, DT_INPUT_DATA);

    AddcmulTilingData *tiling = context->GetTilingData<AddcmulTilingData>();
    tiling->totalLength = totalLength;
    tiling->inputLength = static_cast<uint64_t>(inputShape.GetShapeSize());
    tiling->x1Length = static_cast<uint64_t>(x1Shape.GetShapeSize());
    tiling->x2Length = static_cast<uint64_t>(x2Shape.GetShapeSize());
    tiling->rank = rank;
    tiling->sameShape = ShapeSizeEquals(&inputShape, totalLength) &&
                        ShapeSizeEquals(&x1Shape, totalLength) &&
                        ShapeSizeEquals(&x2Shape, totalLength);

    for (uint32_t i = 0; i < ADDCMUL_MAX_DIMS; ++i) {
        tiling->outShape[i] = i < rank ? outShape[i] : 1;
    }
    FillAlignedShapeAndStride(&inputShape, rank, tiling->inputShape, tiling->inputStride);
    FillAlignedShapeAndStride(&x1Shape, rank, tiling->x1Shape, tiling->x1Stride);
    FillAlignedShapeAndStride(&x2Shape, rank, tiling->x2Shape, tiling->x2Stride);

    uint32_t blockDim = 1;
    if (totalLength > 0 && numCoresAiv > 0) {
        const uint64_t needCores = (totalLength + MIN_ELEMS_PER_CORE - 1) / MIN_ELEMS_PER_CORE;
        blockDim = static_cast<uint32_t>(MinU64(numCoresAiv, MaxU64(needCores, 1)));
    }
    tiling->blockDim = blockDim;
    context->SetBlockDim(blockDim);

    size_t *currentWorkspace = context->GetWorkspaceSizes(1);
    currentWorkspace[0] = 0;
    return ge::GRAPH_SUCCESS;
}
}  // namespace optiling

namespace ge {
static graphStatus InferShape(gert::InferShapeContext *context) {
    const gert::Shape *inputShape = context->GetInputShape(0);
    const gert::Shape *x1Shape = context->GetInputShape(1);
    const gert::Shape *x2Shape = context->GetInputShape(2);
    gert::Shape *yShape = context->GetOutputShape(0);

    uint64_t outShape[ADDCMUL_MAX_DIMS] = {0};
    uint32_t rank = 0;
    if (!CalcBroadcastShape(inputShape, x1Shape, x2Shape, outShape, rank)) {
        return GRAPH_FAILED;
    }

    yShape->SetDimNum(rank);
    for (uint32_t i = 0; i < rank; ++i) {
        yShape->SetDim(i, static_cast<int64_t>(outShape[i]));
    }
    return GRAPH_SUCCESS;
}

static graphStatus InferDataType(gert::InferDataTypeContext *context) {
    context->SetOutputDataType(0, context->GetInputDataType(0));
    return ge::GRAPH_SUCCESS;
}
}  // namespace ge

namespace ops {
class Addcmul : public OpDef {
public:
    explicit Addcmul(const char *name) : OpDef(name) {
        this->Input("input_data")
            .ParamType(REQUIRED)
            .DataType({ge::DT_FLOAT16, ge::DT_FLOAT, ge::DT_INT8, ge::DT_INT32})
            .Format({ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND});
        this->Input("x1")
            .ParamType(REQUIRED)
            .DataType({ge::DT_FLOAT16, ge::DT_FLOAT, ge::DT_INT8, ge::DT_INT32})
            .Format({ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND});
        this->Input("x2")
            .ParamType(REQUIRED)
            .DataType({ge::DT_FLOAT16, ge::DT_FLOAT, ge::DT_INT8, ge::DT_INT32})
            .Format({ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND});
        this->Input("value")
            .ParamType(REQUIRED)
            .DataType({ge::DT_FLOAT16, ge::DT_FLOAT, ge::DT_INT8, ge::DT_INT32})
            .Format({ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND});
        this->Output("y")
            .ParamType(REQUIRED)
            .DataType({ge::DT_FLOAT16, ge::DT_FLOAT, ge::DT_INT8, ge::DT_INT32})
            .Format({ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND});
        this->SetInferShape(ge::InferShape).SetInferDataType(ge::InferDataType);
        this->AICore()
            .SetTiling(optiling::TilingFunc)
            .AddConfig("ascend910b");
    }
};
OP_ADD(Addcmul);
}  // namespace ops
