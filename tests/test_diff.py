import numpy as np
import pytest

from yuvdiff.diff import DiffEngine, DiffResult
from yuvdiff.formats import BitDepth, PixelFormat
from yuvdiff.parser import YUVFrame


def make_frame(y, u=None, v=None, w=4, h=4, bit_depth=8, fmt=PixelFormat.YUV420P):
    if u is None:
        u = np.zeros((h // 2, w // 2), dtype=np.uint8)
    if v is None:
        v = np.zeros((h // 2, w // 2), dtype=np.uint8)
    return YUVFrame(
        y=np.asarray(y, dtype=np.uint8),
        u=np.asarray(u, dtype=np.uint8),
        v=np.asarray(v, dtype=np.uint8),
        bit_depth=bit_depth,
        width=w,
        height=h,
        pixel_format=fmt,
    )


class TestDiffBasic:
    def test_identical_frames_zero_diff(self):
        y = np.full((4, 4), 100, dtype=np.uint8)
        u = np.full((2, 2), 50, dtype=np.uint8)
        v = np.full((2, 2), 50, dtype=np.uint8)
        a = make_frame(y, u, v)
        b = make_frame(y, u, v)
        engine = DiffEngine(threshold=4)
        result = engine.diff(a, b)
        assert isinstance(result, DiffResult)
        assert result.diff_y.sum() == 0
        assert result.diff_u.sum() == 0
        assert result.diff_v.sum() == 0
        assert result.mask.sum() == 0
        assert result.diff_pixel_count == 0
        assert result.total_pixel_count == 16

    def test_black_vs_white_full_diff(self):
        a = make_frame(np.zeros((4, 4), dtype=np.uint8))
        b = make_frame(np.full((4, 4), 255, dtype=np.uint8))
        engine = DiffEngine(threshold=4)
        result = engine.diff(a, b)
        assert result.diff_y.max() == 255
        assert result.mask.all()
        assert result.diff_pixel_count == 16

    def test_threshold_boundary_excludes_equal(self):
        y_a = np.zeros((4, 4), dtype=np.uint8)
        y_b = np.full((4, 4), 4, dtype=np.uint8)  # diff = 4 == threshold
        a = make_frame(y_a)
        b = make_frame(y_b)
        engine = DiffEngine(threshold=4)
        result = engine.diff(a, b)
        assert result.diff_y.max() == 4
        assert result.mask.sum() == 0

    def test_threshold_boundary_includes_above(self):
        y_a = np.zeros((4, 4), dtype=np.uint8)
        y_b = np.full((4, 4), 5, dtype=np.uint8)  # diff = 5 > threshold
        a = make_frame(y_a)
        b = make_frame(y_b)
        engine = DiffEngine(threshold=4)
        result = engine.diff(a, b)
        assert result.mask.sum() == 16


class TestDiffPerChannel:
    def test_only_y_differs(self):
        y_a = np.zeros((4, 4), dtype=np.uint8)
        y_b = np.full((4, 4), 100, dtype=np.uint8)
        u = np.full((2, 2), 50, dtype=np.uint8)
        v = np.full((2, 2), 50, dtype=np.uint8)
        a = make_frame(y_a, u, v)
        b = make_frame(y_b, u, v)
        engine = DiffEngine(threshold=4)
        result = engine.diff(a, b)
        assert result.diff_u.sum() == 0
        assert result.diff_v.sum() == 0
        assert result.diff_y.max() == 100
        assert result.mask.sum() == 16

    def test_chroma_diff_upsampled_to_y_resolution(self):
        y = np.zeros((4, 4), dtype=np.uint8)
        u_a = np.zeros((2, 2), dtype=np.uint8)
        u_b = np.array([[100, 0], [0, 0]], dtype=np.uint8)
        v = np.zeros((2, 2), dtype=np.uint8)
        a = make_frame(y, u_a, v)
        b = make_frame(y, u_b, v)
        engine = DiffEngine(threshold=4)
        result = engine.diff(a, b)
        assert result.mask[:2, :2].all()
        assert not result.mask[:2, 2:].any()
        assert not result.mask[2:, :].any()
        assert result.diff_pixel_count == 4


class TestDiffShape:
    def test_mask_shape_matches_y(self):
        y = np.zeros((4, 4), dtype=np.uint8)
        a = make_frame(y)
        b = make_frame(y + 1)
        engine = DiffEngine(threshold=0)
        result = engine.diff(a, b)
        assert result.mask.shape == (4, 4)
