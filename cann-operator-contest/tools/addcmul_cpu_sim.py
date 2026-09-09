"""CPU simulator for the AddCMul CANN operator template.

Run:
    python tools/addcmul_cpu_sim.py
    python tools/addcmul_cpu_sim.py --demo

This file is intentionally dependency-free. It helps practice broadcast,
stride, core splitting, and tail-tile handling before a real CANN environment
is available.
"""

from __future__ import annotations

import argparse
import math
import struct
from dataclasses import dataclass
from typing import Iterable, Sequence


MAX_DIMS = 8


class BroadcastError(ValueError):
    pass


@dataclass(frozen=True)
class Tiling:
    out_shape: tuple[int, ...]
    input_shape: tuple[int, ...]
    x1_shape: tuple[int, ...]
    x2_shape: tuple[int, ...]
    input_stride: tuple[int, ...]
    x1_stride: tuple[int, ...]
    x2_stride: tuple[int, ...]
    total_length: int
    same_shape: bool
    block_dim: int


def prod(values: Iterable[int]) -> int:
    total = 1
    for value in values:
        total *= value
    return total


def quantize_float32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", float(value)))[0]


def quantize_float16(value: float) -> float:
    return struct.unpack("e", struct.pack("e", float(value)))[0]


def to_int8(value: int) -> int:
    value = int(value)
    if value < -128 or value > 127:
        raise OverflowError(f"int8 result out of range: {value}")
    return value


def broadcast_dim(dims: Sequence[int]) -> int:
    if any(dim < 0 for dim in dims):
        raise BroadcastError(f"negative dimension is invalid: {dims}")

    non_one = [dim for dim in dims if dim != 1]
    if not non_one:
        return 1

    if 0 in non_one:
        if any(dim not in (0, 1) for dim in dims):
            raise BroadcastError(f"cannot broadcast dimensions: {dims}")
        return 0

    dim = non_one[0]
    if any(other != dim for other in non_one):
        raise BroadcastError(f"cannot broadcast dimensions: {dims}")
    return dim


def broadcast_shape(*shapes: Sequence[int]) -> tuple[int, ...]:
    rank = max(len(shape) for shape in shapes)
    if rank > MAX_DIMS:
        raise BroadcastError(f"rank {rank} exceeds MAX_DIMS={MAX_DIMS}")

    out: list[int] = []
    for out_dim in range(rank):
        dims = []
        for shape in shapes:
            in_rank = len(shape)
            if out_dim + in_rank >= rank:
                dims.append(int(shape[out_dim + in_rank - rank]))
            else:
                dims.append(1)
        out.append(broadcast_dim(dims))
    return tuple(out)


def align_shape(shape: Sequence[int], rank: int) -> tuple[int, ...]:
    if len(shape) > rank:
        raise ValueError("input rank cannot exceed output rank")
    return (1,) * (rank - len(shape)) + tuple(int(dim) for dim in shape)


def calc_stride(shape: Sequence[int]) -> tuple[int, ...]:
    stride = [0] * len(shape)
    cur = 1
    for i in range(len(shape) - 1, -1, -1):
        stride[i] = cur
        cur *= int(shape[i])
    return tuple(stride)


def split_core_range(total_length: int, block_dim: int, block_idx: int) -> tuple[int, int]:
    if block_dim <= 0:
        raise ValueError("block_dim must be positive")
    if block_idx < 0 or block_idx >= block_dim:
        raise ValueError("block_idx out of range")

    base_len = total_length // block_dim
    tail = total_length % block_dim
    if block_idx < tail:
        core_len = base_len + 1
        core_start = block_idx * core_len
    else:
        core_len = base_len
        core_start = tail * (base_len + 1) + (block_idx - tail) * base_len
    return core_start, core_len


def calc_broadcast_offset(
    out_index: int,
    out_shape: Sequence[int],
    input_shape: Sequence[int],
    input_stride: Sequence[int],
) -> int:
    offset = 0
    remain = out_index
    for dim in range(len(out_shape) - 1, -1, -1):
        out_dim = int(out_shape[dim])
        coord = 0 if out_dim == 0 else remain % out_dim
        remain = 0 if out_dim == 0 else remain // out_dim

        if int(input_shape[dim]) != 1:
            offset += coord * int(input_stride[dim])
    return offset


def choose_block_dim(total_length: int, max_cores: int = 20, min_elems_per_core: int = 256) -> int:
    if total_length <= 0:
        return 1
    need_cores = math.ceil(total_length / min_elems_per_core)
    return max(1, min(max_cores, need_cores))


def make_tiling(
    input_shape: Sequence[int],
    x1_shape: Sequence[int],
    x2_shape: Sequence[int],
    max_cores: int = 20,
) -> Tiling:
    out_shape = broadcast_shape(input_shape, x1_shape, x2_shape)
    rank = len(out_shape)
    aligned_input = align_shape(input_shape, rank)
    aligned_x1 = align_shape(x1_shape, rank)
    aligned_x2 = align_shape(x2_shape, rank)
    total_length = prod(out_shape)

    return Tiling(
        out_shape=out_shape,
        input_shape=aligned_input,
        x1_shape=aligned_x1,
        x2_shape=aligned_x2,
        input_stride=calc_stride(aligned_input),
        x1_stride=calc_stride(aligned_x1),
        x2_stride=calc_stride(aligned_x2),
        total_length=total_length,
        same_shape=prod(aligned_input) == total_length
        and prod(aligned_x1) == total_length
        and prod(aligned_x2) == total_length,
        block_dim=choose_block_dim(total_length, max_cores=max_cores),
    )


def calc_scalar(input_value, x1_value, x2_value, value, dtype: str):
    if dtype == "float32":
        result = (
            quantize_float32(input_value)
            + quantize_float32(x1_value) * quantize_float32(x2_value) * quantize_float32(value)
        )
        return quantize_float32(result)
    if dtype == "float16":
        result = (
            quantize_float16(input_value)
            + quantize_float16(x1_value) * quantize_float16(x2_value) * quantize_float16(value)
        )
        return quantize_float16(result)
    if dtype == "int32":
        return int(input_value) + int(x1_value) * int(x2_value) * int(value)
    if dtype == "int8":
        return to_int8(int(input_value) + int(x1_value) * int(x2_value) * int(value))
    raise ValueError(f"unsupported dtype: {dtype}")


def addcmul_kernel_sim(
    input_data: Sequence,
    input_shape: Sequence[int],
    x1: Sequence,
    x1_shape: Sequence[int],
    x2: Sequence,
    x2_shape: Sequence[int],
    value: Sequence,
    dtype: str = "float32",
    tile_len: int = 1024,
    max_cores: int = 20,
) -> tuple[list, Tiling]:
    if len(value) != 1:
        raise ValueError("value must contain exactly one scalar element")

    tiling = make_tiling(input_shape, x1_shape, x2_shape, max_cores=max_cores)
    if len(input_data) != prod(input_shape):
        raise ValueError("input_data length does not match input_shape")
    if len(x1) != prod(x1_shape):
        raise ValueError("x1 length does not match x1_shape")
    if len(x2) != prod(x2_shape):
        raise ValueError("x2 length does not match x2_shape")

    y = [0] * tiling.total_length
    for block_idx in range(tiling.block_dim):
        core_start, core_len = split_core_range(tiling.total_length, tiling.block_dim, block_idx)
        processed = 0
        while processed < core_len:
            cur_len = min(tile_len, core_len - processed)
            for local_idx in range(cur_len):
                out_index = core_start + processed + local_idx
                input_offset = calc_broadcast_offset(
                    out_index, tiling.out_shape, tiling.input_shape, tiling.input_stride
                )
                x1_offset = calc_broadcast_offset(out_index, tiling.out_shape, tiling.x1_shape, tiling.x1_stride)
                x2_offset = calc_broadcast_offset(out_index, tiling.out_shape, tiling.x2_shape, tiling.x2_stride)

                y[out_index] = calc_scalar(
                    input_data[input_offset],
                    x1[x1_offset],
                    x2[x2_offset],
                    value[0],
                    dtype,
                )
            processed += cur_len

    return y, tiling


def assert_close(actual: Sequence, expected: Sequence, dtype: str, name: str) -> None:
    if len(actual) != len(expected):
        raise AssertionError(f"{name}: length mismatch {len(actual)} != {len(expected)}")

    if dtype in ("int8", "int32"):
        if list(actual) != list(expected):
            raise AssertionError(f"{name}: {actual} != {expected}")
        return

    atol = 1e-3 if dtype == "float16" else 1e-4
    rtol = 1e-3 if dtype == "float16" else 1e-4
    for i, (lhs, rhs) in enumerate(zip(actual, expected)):
        diff = abs(lhs - rhs)
        limit = atol + rtol * abs(rhs)
        if diff > limit:
            raise AssertionError(f"{name}: index {i}, {lhs} != {rhs}, diff={diff}, limit={limit}")


def run_tests() -> None:
    y, tiling = addcmul_kernel_sim(
        [1.0, 2.0, 3.0],
        [3],
        [2.0, 3.0, 4.0],
        [3],
        [3.0, 4.0, 5.0],
        [3],
        [0.5],
        dtype="float32",
        tile_len=2,
        max_cores=2,
    )
    assert_close(y, [4.0, 8.0, 13.0], "float32", "basic float32")
    assert tiling.same_shape

    y, tiling = addcmul_kernel_sim(
        [1.0, 2.0, 3.0, 4.0],
        [2, 2],
        [2.0, 3.0, 4.0, 5.0],
        [2, 2],
        [1.0, 2.0],
        [2],
        [1.0],
        dtype="float32",
        tile_len=3,
        max_cores=3,
    )
    assert_close(y, [3.0, 8.0, 7.0, 14.0], "float32", "broadcast x2")
    assert not tiling.same_shape

    y, _ = addcmul_kernel_sim(
        [10, 20, 30],
        [3],
        [2, 3, 4],
        [3],
        [3, 4, 5],
        [3],
        [2],
        dtype="int32",
        tile_len=2,
        max_cores=2,
    )
    assert_close(y, [22, 44, 70], "int32", "int32")

    shape = [3, 4, 19]
    total = prod(shape)
    input_data = [float((i % 17) - 8) for i in range(total)]
    x1 = [float((i % 7) - 3) for i in range(total)]
    x2 = [float((i % 5) - 2) for i in range(total)]
    y, tiling = addcmul_kernel_sim(input_data, shape, x1, shape, x2, shape, [0.25], "float32", 32, 5)
    expected = [calc_scalar(input_data[i], x1[i], x2[i], 0.25, "float32") for i in range(total)]
    assert_close(y, expected, "float32", "non-aligned tail")
    assert tiling.total_length == 228

    y, tiling = addcmul_kernel_sim(
        [1.0] * 120,
        [2, 3, 4, 5],
        [2.0] * 15,
        [1, 3, 1, 5],
        [3.0] * 4,
        [4, 1],
        [0.5],
        dtype="float16",
        tile_len=17,
        max_cores=4,
    )
    assert len(y) == 120
    assert_close(y[:8], [4.0] * 8, "float16", "high-dim broadcast float16")
    assert not tiling.same_shape

    y, tiling = addcmul_kernel_sim([], [0, 3], [], [0, 3], [], [0, 3], [1.0], "float32")
    assert y == []
    assert tiling.total_length == 0

    print("All AddCMul CPU simulator tests passed.")


def run_demo() -> None:
    _, tiling = addcmul_kernel_sim(
        [1.0] * 24,
        [2, 3, 4],
        [2.0] * 4,
        [1, 1, 4],
        [3.0] * 12,
        [3, 4],
        [0.5],
        dtype="float32",
        tile_len=5,
        max_cores=4,
    )
    print("Demo tiling:")
    print(f"  out_shape    = {tiling.out_shape}")
    print(f"  input_shape  = {tiling.input_shape}, stride = {tiling.input_stride}")
    print(f"  x1_shape     = {tiling.x1_shape}, stride = {tiling.x1_stride}")
    print(f"  x2_shape     = {tiling.x2_shape}, stride = {tiling.x2_stride}")
    print(f"  total_length = {tiling.total_length}")
    print(f"  same_shape   = {tiling.same_shape}")
    print(f"  block_dim    = {tiling.block_dim}")
    for block_idx in range(tiling.block_dim):
        start, length = split_core_range(tiling.total_length, tiling.block_dim, block_idx)
        print(f"  core {block_idx}: start={start}, length={length}")
    print("  first 8 output-index mappings:")
    for out_index in range(min(8, tiling.total_length)):
        print(
            "    out[{out}] <- input[{inp}], x1[{x1}], x2[{x2}]".format(
                out=out_index,
                inp=calc_broadcast_offset(out_index, tiling.out_shape, tiling.input_shape, tiling.input_stride),
                x1=calc_broadcast_offset(out_index, tiling.out_shape, tiling.x1_shape, tiling.x1_stride),
                x2=calc_broadcast_offset(out_index, tiling.out_shape, tiling.x2_shape, tiling.x2_stride),
            )
        )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--demo", action="store_true", help="print one broadcast/tiling demo")
    args = parser.parse_args()

    if args.demo:
        run_demo()
    else:
        run_tests()


if __name__ == "__main__":
    main()
