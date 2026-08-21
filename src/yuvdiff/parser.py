"""YUV file parser with mmap-backed lazy frame reads.

Reads planar YUV files (YUV420P/422P/444P, 8-bit or 10-bit MSB-aligned LE).
Only the requested frame is read into memory; the underlying file is
mmap'd for large files (>1 MB) to avoid loading the whole sequence.
"""
from __future__ import annotations

import mmap
import os
from dataclasses import dataclass
from typing import Optional

import numpy as np

from yuvdiff.formats import (
    BitDepth,
    PixelFormat,
    chroma_subsampling,
    frame_bytes,
    parse_format,
)


@dataclass
class YUVFrame:
    """A single YUV frame, planar layout."""

    y: np.ndarray
    u: np.ndarray
    v: np.ndarray
    bit_depth: int
    width: int
    height: int
    pixel_format: PixelFormat


class YUVParser:
    """mmap-backed reader for a raw YUV file.

    Frames are read lazily; only the requested frame's bytes are copied
    out of the mmap window into a numpy array.
    """

    _MMAP_THRESHOLD = 1 << 20  # 1 MB

    def __init__(self, path: str, fmt: str, width: int, height: int):
        if not os.path.exists(path):
            raise FileNotFoundError(f"YUV file not found: {path}")
        self.path = path
        self.width = width
        self.height = height
        self.pixel_format, self.bit_depth = parse_format(fmt)
        self._frame_bytes = frame_bytes(
            self.pixel_format, width, height, self.bit_depth
        )
        self._file_size = os.path.getsize(path)
        self.num_frames = self._file_size // self._frame_bytes
        self._mmap: Optional[mmap.mmap] = None
        if self._file_size >= self._MMAP_THRESHOLD:
            with open(path, "rb") as f:
                self._mmap = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)

    def read_frame(self, idx: int) -> YUVFrame:
        if idx < 0 or idx >= self.num_frames:
            raise IndexError(
                f"Frame {idx} out of range [0, {self.num_frames})"
            )
        h_f, v_f = chroma_subsampling(self.pixel_format)
        cy, cx = self.height // v_f, self.width // h_f

        if self.bit_depth == BitDepth.BIT8:
            return self._read_8bit(idx, cy, cx)
        return self._read_10bit(idx, cy, cx)

    def _read_8bit(self, idx: int, cy: int, cx: int) -> YUVFrame:
        offset = idx * self._frame_bytes
        y_size = self.width * self.height
        uv_size = cy * cx
        y = self._view(offset, y_size, np.uint8).reshape(self.height, self.width)
        u = self._view(offset + y_size, uv_size, np.uint8).reshape(cy, cx)
        v = self._view(
            offset + y_size + uv_size, uv_size, np.uint8
        ).reshape(cy, cx)
        return YUVFrame(
            y=y.copy(),
            u=u.copy(),
            v=v.copy(),
            bit_depth=8,
            width=self.width,
            height=self.height,
            pixel_format=self.pixel_format,
        )

    def _read_10bit(self, idx: int, cy: int, cx: int) -> YUVFrame:
        offset = idx * self._frame_bytes
        y_bytes = self.width * self.height * 2
        uv_bytes = cy * cx * 2
        y_raw = self._view(offset, y_bytes, np.uint16).reshape(
            self.height, self.width
        )
        u_raw = self._view(offset + y_bytes, uv_bytes, np.uint16).reshape(cy, cx)
        v_raw = self._view(
            offset + y_bytes + uv_bytes, uv_bytes, np.uint16
        ).reshape(cy, cx)
        return YUVFrame(
            y=(y_raw >> 6).astype(np.uint16),
            u=(u_raw >> 6).astype(np.uint16),
            v=(v_raw >> 6).astype(np.uint16),
            bit_depth=10,
            width=self.width,
            height=self.height,
            pixel_format=self.pixel_format,
        )

    def _view(self, offset: int, count_bytes: int, dtype) -> np.ndarray:
        """Return a numpy view of `count_bytes` starting at `offset`.

        For mmap files this is a zero-copy slice; for small files this is
        a fresh read.
        """
        if self._mmap is not None:
            return np.frombuffer(
                self._mmap[offset : offset + count_bytes], dtype=dtype
            )
        with open(self.path, "rb") as f:
            f.seek(offset)
            return np.frombuffer(f.read(count_bytes), dtype=dtype)

    def close(self) -> None:
        if self._mmap is not None:
            self._mmap.close()
            self._mmap = None

    def __enter__(self) -> "YUVParser":
        return self

    def __exit__(self, *args) -> None:
        self.close()
