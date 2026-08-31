"""Renderer tests.

These tests require PySide6 to be importable. On systems without
libEGL/EGL support (some headless sandboxes), these tests are skipped.
"""
import numpy as np
import pytest

try:
    from PySide6.QtGui import QImage
    from PySide6.QtWidgets import QApplication
    _QT_AVAILABLE = True
except ImportError:
    _QT_AVAILABLE = False
    QImage = None
    QApplication = None

from yuvdiff.diff import DiffEngine
from yuvdiff.formats import PixelFormat
from yuvdiff.parser import YUVFrame
from yuvdiff.renderer import Renderer, RenderMode


pytestmark = pytest.mark.skipif(
    not _QT_AVAILABLE, reason="PySide6 / libEGL not available in this env"
)


def make_frame(y, u, v, w, h, bit_depth=8, fmt=PixelFormat.YUV420P):
    dtype = np.uint8 if bit_depth == 8 else np.uint16
    return YUVFrame(
        y=np.asarray(y, dtype=dtype),
        u=np.asarray(u, dtype=dtype),
        v=np.asarray(v, dtype=dtype),
        bit_depth=bit_depth,
        width=w, height=h, pixel_format=fmt,
    )


@pytest.fixture(scope="module")
def qapp():
    if not _QT_AVAILABLE:
        pytest.skip("Qt not available")
    import os
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    app = QApplication.instance() or QApplication([])
    yield app


class TestRendererOriginal:
    def test_original_a_returns_qimage(self, qapp):
        y = np.full((16, 16), 128, dtype=np.uint8)
        u = np.full((8, 8), 128, dtype=np.uint8)
        v = np.full((8, 8), 128, dtype=np.uint8)
        a = make_frame(y, u, v, 16, 16)
        r = Renderer(16, 16)
        img = r.render(a, None, None, RenderMode.ORIGINAL_A, threshold=4)
        assert isinstance(img, QImage)
        assert img.width() == 16
        assert img.height() == 16

    def test_original_b_returns_qimage(self, qapp):
        y = np.full((16, 16), 64, dtype=np.uint8)
        u = np.full((8, 8), 64, dtype=np.uint8)
        v = np.full((8, 8), 64, dtype=np.uint8)
        a = make_frame(y, u, v, 16, 16)
        b = make_frame(np.full((16, 16), 200, dtype=np.uint8),
                       np.full((8, 8), 200, dtype=np.uint8),
                       np.full((8, 8), 200, dtype=np.uint8), 16, 16)
        r = Renderer(16, 16)
        img = r.render(a, b, None, RenderMode.ORIGINAL_B, threshold=4)
        assert img.width() == 16


class TestRendererHeatmap:
    def test_heatmap_returns_qimage(self, qapp):
        y = np.zeros((16, 16), dtype=np.uint8)
        a = make_frame(y, np.zeros((8, 8), dtype=np.uint8), np.zeros((8, 8), dtype=np.uint8), 16, 16)
        b = make_frame(np.full((16, 16), 100, dtype=np.uint8),
                       np.full((8, 8), 50, dtype=np.uint8),
                       np.full((8, 8), 50, dtype=np.uint8), 16, 16)
        diff = DiffEngine(threshold=4).diff(a, b)
        r = Renderer(16, 16)
        img = r.render(a, b, diff, RenderMode.HEATMAP, threshold=4)
        assert img.width() == 16
        assert img.height() == 16


class TestRendererThresholdMask:
    def test_mask_marks_diff_pixels(self, qapp):
        y = np.zeros((16, 16), dtype=np.uint8)
        a = make_frame(y, np.zeros((8, 8), dtype=np.uint8), np.zeros((8, 8), dtype=np.uint8), 16, 16)
        b_y = y.copy()
        b_y[:8, :] = 100
        b = make_frame(b_y, np.zeros((8, 8), dtype=np.uint8), np.zeros((8, 8), dtype=np.uint8), 16, 16)
        diff = DiffEngine(threshold=4).diff(a, b)
        r = Renderer(16, 16)
        img = r.render(a, b, diff, RenderMode.THRESHOLD_MASK, threshold=4)
        assert img.width() == 16
        top_pixel = img.pixelColor(4, 4)
        bottom_pixel = img.pixelColor(4, 12)
        assert top_pixel.red() > top_pixel.green()
        assert top_pixel.red() > top_pixel.blue()
        assert abs(bottom_pixel.red() - bottom_pixel.blue()) < 30


class TestRendererSideBySide:
    def test_side_by_side_returns_double_width(self, qapp):
        y = np.full((16, 16), 128, dtype=np.uint8)
        u = np.full((8, 8), 128, dtype=np.uint8)
        v = np.full((8, 8), 128, dtype=np.uint8)
        a = make_frame(y, u, v, 16, 16)
        b = make_frame(y, u, v, 16, 16)
        r = Renderer(16, 16)
        img = r.render(a, b, None, RenderMode.SIDE_BY_SIDE, threshold=4)
        assert isinstance(img, QImage)
        assert img.width() == 32
        assert img.height() == 16


class TestRendererComparison:
    def test_comparison_returns_qimage(self, qapp):
        y = np.full((16, 16), 128, dtype=np.uint8)
        u = np.full((8, 8), 128, dtype=np.uint8)
        v = np.full((8, 8), 128, dtype=np.uint8)
        a = make_frame(y, u, v, 16, 16)
        b = make_frame(y, u, v, 16, 16)
        r = Renderer(16, 16)
        img = r.render(a, b, None, RenderMode.COMPARISON, threshold=4)
        assert isinstance(img, QImage)
        assert img.width() == 16
        assert img.height() == 16

