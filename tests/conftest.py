"""Shared pytest fixtures for yuvdiff tests."""
import numpy as np
import pytest

from yuvdiff.formats import BitDepth, PixelFormat, frame_bytes


@pytest.fixture
def rng():
    """Deterministic random generator so test failures are reproducible."""
    return np.random.default_rng(seed=20260821)


def _write_synthetic_yuv(
    path,
    fmt_str: str,
    width: int,
    height: int,
    num_frames: int,
    rng: np.random.Generator,
    alignment: str = "msb",
) -> str:
    """Write a tiny synthetic YUV file and return the format string used.

    `alignment` is only relevant for *10LE formats: "msb" (default) puts
    the 10-bit value in bits [15:6]; "lsb" leaves it in bits [9:0].
    """
    fmt_map = {
        "YUV420P8": (PixelFormat.YUV420P, BitDepth.BIT8, np.uint8),
        "YUV422P8": (PixelFormat.YUV422P, BitDepth.BIT8, np.uint8),
        "YUV444P8": (PixelFormat.YUV444P, BitDepth.BIT8, np.uint8),
        "YUV420P10LE": (PixelFormat.YUV420P, BitDepth.BIT10LE, np.uint16),
        "YUV422P10LE": (PixelFormat.YUV422P, BitDepth.BIT10LE, np.uint16),
        "YUV444P10LE": (PixelFormat.YUV444P, BitDepth.BIT10LE, np.uint16),
    }
    pixel_fmt, bit_depth, dtype = fmt_map[fmt_str]
    h_f, v_f = (2, 2) if pixel_fmt == PixelFormat.YUV420P else (
        (2, 1) if pixel_fmt == PixelFormat.YUV422P else (1, 1)
    )
    cy, cx = height // v_f, width // h_f
    max_val = 255 if bit_depth == BitDepth.BIT8 else 1023

    with open(path, "wb") as f:
        for _ in range(num_frames):
            y = rng.integers(0, max_val + 1, size=(height, width), dtype=dtype)
            u = rng.integers(0, max_val + 1, size=(cy, cx), dtype=dtype)
            v = rng.integers(0, max_val + 1, size=(cy, cx), dtype=dtype)
            if bit_depth == BitDepth.BIT10LE:
                if alignment == "msb":
                    y = (y.astype(np.uint16) << 6)
                    u = (u.astype(np.uint16) << 6)
                    v = (v.astype(np.uint16) << 6)
                # lsb: leave raw 10-bit values, no shift
                f.write(y.astype("<u2").tobytes())
                f.write(u.astype("<u2").tobytes())
                f.write(v.astype("<u2").tobytes())
            else:
                f.write(y.tobytes())
                f.write(u.tobytes())
                f.write(v.tobytes())
    return fmt_str


@pytest.fixture
def synth_yuv_420p8(tmp_path, rng):
    """Synthetic 32x16, 5-frame YUV420P8 file."""
    path = tmp_path / "synth_420p8.yuv"
    fmt = _write_synthetic_yuv(path, "YUV420P8", 32, 16, 5, rng)
    return str(path), fmt, 32, 16, 5


@pytest.fixture
def synth_yuv_422p8(tmp_path, rng):
    path = tmp_path / "synth_422p8.yuv"
    fmt = _write_synthetic_yuv(path, "YUV422P8", 32, 16, 5, rng)
    return str(path), fmt, 32, 16, 5


@pytest.fixture
def synth_yuv_444p8(tmp_path, rng):
    path = tmp_path / "synth_444p8.yuv"
    fmt = _write_synthetic_yuv(path, "YUV444P8", 32, 16, 5, rng)
    return str(path), fmt, 32, 16, 5


@pytest.fixture
def synth_yuv_420p10le(tmp_path, rng):
    path = tmp_path / "synth_420p10le.yuv"
    fmt = _write_synthetic_yuv(path, "YUV420P10LE", 32, 16, 5, rng)
    return str(path), fmt, 32, 16, 5


@pytest.fixture
def synth_yuv_422p10le(tmp_path, rng):
    path = tmp_path / "synth_422p10le.yuv"
    fmt = _write_synthetic_yuv(path, "YUV422P10LE", 32, 16, 5, rng)
    return str(path), fmt, 32, 16, 5


@pytest.fixture
def synth_yuv_444p10le(tmp_path, rng):
    path = tmp_path / "synth_444p10le.yuv"
    fmt = _write_synthetic_yuv(path, "YUV444P10LE", 32, 16, 5, rng)
    return str(path), fmt, 32, 16, 5


@pytest.fixture
def synth_yuv_420p10le_lsb(tmp_path, rng):
    """LSB-aligned 10-bit YUV420P (value in bits [9:0])."""
    path = tmp_path / "synth_420p10le_lsb.yuv"
    _write_synthetic_yuv(path, "YUV420P10LE", 32, 16, 5, rng, alignment="lsb")
    return str(path), "YUV420P10LE", 32, 16, 5
