import math

import numpy as np
import pytest

from yuvdiff.formats import BitDepth, PixelFormat
from yuvdiff.metrics import MetricsCalculator, PSNRResult, SSIMResult
from yuvdiff.parser import YUVFrame


def make_frame(y, u, v, w, h, bit_depth=8, fmt=PixelFormat.YUV420P):
    dtype = np.uint8 if bit_depth == 8 else np.uint16
    return YUVFrame(
        y=np.asarray(y, dtype=dtype),
        u=np.asarray(u, dtype=dtype),
        v=np.asarray(v, dtype=dtype),
        bit_depth=bit_depth,
        width=w,
        height=h,
        pixel_format=fmt,
    )


class TestPSNR:
    def test_identical_frames_yields_inf(self):
        y = np.full((4, 4), 100, dtype=np.uint8)
        u = np.full((2, 2), 50, dtype=np.uint8)
        v = np.full((2, 2), 50, dtype=np.uint8)
        a = make_frame(y, u, v, 4, 4)
        b = make_frame(y, u, v, 4, 4)
        m = MetricsCalculator()
        result = m.psnr(a, b)
        assert isinstance(result, PSNRResult)
        assert math.isinf(result.y)
        assert math.isinf(result.u)
        assert math.isinf(result.v)
        assert math.isinf(result.total)

    def test_known_psnr_constant_diff(self):
        y_a = np.zeros((4, 4), dtype=np.uint8)
        y_b = np.full((4, 4), 10, dtype=np.uint8)
        u = v = np.zeros((2, 2), dtype=np.uint8)
        a = make_frame(y_a, u, v, 4, 4)
        b = make_frame(y_b, u, v, 4, 4)
        m = MetricsCalculator()
        result = m.psnr(a, b)
        # MSE_Y = 100, MAX = 255
        # PSNR_Y = 10 * log10(255^2 / 100) ≈ 28.13
        assert result.y == pytest.approx(28.1308, abs=0.01)
        assert math.isinf(result.u)
        # Total: 420 weighting, MSE_Y=100, MSE_U=MSE_V=0
        # MSE_total = (4*100 + 0 + 0) / 6 = 66.667
        # PSNR = 10 * log10(255^2 / 66.667) ≈ 29.89
        assert result.total == pytest.approx(29.8949, abs=0.01)

    def test_8bit_max_is_255(self):
        y_a = np.zeros((4, 4), dtype=np.uint8)
        y_b = np.full((4, 4), 1, dtype=np.uint8)
        u = v = np.zeros((2, 2), dtype=np.uint8)
        a = make_frame(y_a, u, v, 4, 4)
        b = make_frame(y_b, u, v, 4, 4)
        m = MetricsCalculator()
        result = m.psnr(a, b)
        expected_y = 10 * math.log10(255 ** 2 / 1)
        assert result.y == pytest.approx(expected_y, abs=0.01)

    def test_10bit_max_is_1023(self):
        y_a = np.zeros((4, 4), dtype=np.uint16)
        y_b = np.full((4, 4), 1, dtype=np.uint16)
        u = v = np.zeros((2, 2), dtype=np.uint16)
        a = make_frame(y_a, u, v, 4, 4, bit_depth=10)
        b = make_frame(y_b, u, v, 4, 4, bit_depth=10)
        m = MetricsCalculator()
        result = m.psnr(a, b)
        expected_y = 10 * math.log10(1023 ** 2 / 1)
        assert result.y == pytest.approx(expected_y, abs=0.01)

    def test_yuv422_weighting(self):
        y_a = np.zeros((4, 4), dtype=np.uint8)
        y_b = np.full((4, 4), 10, dtype=np.uint8)
        u = np.full((4, 2), 5, dtype=np.uint8)
        v = np.zeros((4, 2), dtype=np.uint8)
        a = make_frame(y_a, np.zeros_like(u), np.zeros_like(v), 4, 4, fmt=PixelFormat.YUV422P)
        b = make_frame(y_b, u, v, 4, 4, fmt=PixelFormat.YUV422P)
        m = MetricsCalculator()
        result = m.psnr(a, b)
        # MSE_Y = 100, MSE_U = 25, MSE_V = 0
        # MSE_total_422 = (2*100 + 25 + 0) / 4 = 56.25
        # PSNR = 10 * log10(255^2 / 56.25) ≈ 30.63
        assert result.total == pytest.approx(30.6267, abs=0.01)

    def test_yuv444_weighting(self):
        y_a = np.zeros((2, 2), dtype=np.uint8)
        y_b = np.full((2, 2), 10, dtype=np.uint8)
        u_a = np.zeros((2, 2), dtype=np.uint8)
        u_b = np.full((2, 2), 5, dtype=np.uint8)
        v = np.zeros((2, 2), dtype=np.uint8)
        a = make_frame(y_a, u_a, v, 2, 2, fmt=PixelFormat.YUV444P)
        b = make_frame(y_b, u_b, v, 2, 2, fmt=PixelFormat.YUV444P)
        m = MetricsCalculator()
        result = m.psnr(a, b)
        # MSE_Y = 100, MSE_U = 25, MSE_V = 0
        # MSE_total_444 = (100 + 25 + 0) / 3 = 41.667
        # PSNR = 10 * log10(255^2 / 41.667) ≈ 31.94
        assert result.total == pytest.approx(31.9386, abs=0.01)


class TestSSIM:
    def test_identical_frames_yields_one(self):
        y = np.random.default_rng(0).integers(0, 256, size=(32, 32), dtype=np.uint8)
        u = np.zeros((16, 16), dtype=np.uint8)
        v = np.zeros((16, 16), dtype=np.uint8)
        a = make_frame(y, u, v, 32, 32)
        b = make_frame(y, u, v, 32, 32)
        m = MetricsCalculator()
        result = m.ssim(a, b)
        assert isinstance(result, SSIMResult)
        assert result.y == pytest.approx(1.0, abs=1e-6)

    def test_constant_different_frames_low_ssim(self):
        y_a = np.zeros((32, 32), dtype=np.uint8)
        y_b = np.full((32, 32), 255, dtype=np.uint8)
        u = v = np.zeros((16, 16), dtype=np.uint8)
        a = make_frame(y_a, u, v, 32, 32)
        b = make_frame(y_b, u, v, 32, 32)
        m = MetricsCalculator()
        result = m.ssim(a, b)
        assert result.y < 0.1

    def test_small_diff_high_ssim(self):
        rng = np.random.default_rng(0)
        y = rng.integers(50, 200, size=(64, 64), dtype=np.uint8)
        y2 = np.clip(y.astype(np.int16) + 1, 0, 255).astype(np.uint8)
        u = v = np.zeros((32, 32), dtype=np.uint8)
        a = make_frame(y, u, v, 64, 64)
        b = make_frame(y2, u, v, 64, 64)
        m = MetricsCalculator()
        result = m.ssim(a, b)
        assert result.y > 0.99

    def test_8bit_constants(self):
        y = np.random.default_rng(0).integers(0, 256, size=(32, 32), dtype=np.uint8)
        u = v = np.zeros((16, 16), dtype=np.uint8)
        a = make_frame(y, u, v, 32, 32)
        b = make_frame(y, u, v, 32, 32)
        m = MetricsCalculator()
        result = m.ssim(a, b)
        assert result.y == pytest.approx(1.0, abs=1e-6)

    def test_10bit_constants(self):
        y = np.random.default_rng(0).integers(0, 1024, size=(32, 32), dtype=np.uint16)
        u = v = np.zeros((16, 16), dtype=np.uint16)
        a = make_frame(y, u, v, 32, 32, bit_depth=10)
        b = make_frame(y, u, v, 32, 32, bit_depth=10)
        m = MetricsCalculator()
        result = m.ssim(a, b)
        assert result.y == pytest.approx(1.0, abs=1e-6)
