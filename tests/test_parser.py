import os
import pytest

from yuvdiff.parser import YUVParser, YUVFrame
from tests.conftest import _write_synthetic_yuv


class TestParserBasic:
    def test_num_frames_matches_file_size(self, synth_yuv_420p8):
        path, fmt, w, h, n = synth_yuv_420p8
        p = YUVParser(path, fmt, w, h)
        assert p.num_frames == n

    def test_read_frame_returns_correct_shapes(self, synth_yuv_420p8):
        path, fmt, w, h, n = synth_yuv_420p8
        p = YUVParser(path, fmt, w, h)
        frame = p.read_frame(0)
        assert isinstance(frame, YUVFrame)
        assert frame.y.shape == (h, w)
        assert frame.u.shape == (h // 2, w // 2)
        assert frame.v.shape == (h // 2, w // 2)
        assert frame.y.dtype == "uint8"

    def test_read_last_frame(self, synth_yuv_420p8):
        path, fmt, w, h, n = synth_yuv_420p8
        p = YUVParser(path, fmt, w, h)
        frame = p.read_frame(n - 1)
        assert frame.y.shape == (h, w)

    def test_out_of_bounds_raises(self, synth_yuv_420p8):
        path, fmt, w, h, n = synth_yuv_420p8
        p = YUVParser(path, fmt, w, h)
        with pytest.raises(IndexError):
            p.read_frame(n)


class TestParserFormats:
    @pytest.mark.parametrize(
        "fixture_name,expected_y_dtype,expected_bit_depth",
        [
            ("synth_yuv_420p8", "uint8", 8),
            ("synth_yuv_422p8", "uint8", 8),
            ("synth_yuv_444p8", "uint8", 8),
            ("synth_yuv_420p10le", "uint16", 10),
            ("synth_yuv_422p10le", "uint16", 10),
            ("synth_yuv_444p10le", "uint16", 10),
        ],
    )
    def test_all_formats_read_correctly(
        self, request, fixture_name, expected_y_dtype, expected_bit_depth
    ):
        path, fmt, w, h, n = request.getfixturevalue(fixture_name)
        p = YUVParser(path, fmt, w, h)
        assert p.num_frames == n
        frame = p.read_frame(0)
        assert frame.y.dtype == expected_y_dtype
        assert frame.bit_depth == expected_bit_depth

    def test_10bit_values_in_valid_range(self, synth_yuv_420p10le):
        path, fmt, w, h, n = synth_yuv_420p10le
        p = YUVParser(path, fmt, w, h)
        frame = p.read_frame(0)
        assert frame.y.min() >= 0
        assert frame.y.max() <= 1023
        assert frame.y.max() > 100

    def test_yuv422_chroma_shapes(self, synth_yuv_422p8):
        path, fmt, w, h, n = synth_yuv_422p8
        p = YUVParser(path, fmt, w, h)
        frame = p.read_frame(0)
        assert frame.u.shape == (h, w // 2)
        assert frame.v.shape == (h, w // 2)

    def test_yuv444_chroma_shapes(self, synth_yuv_444p8):
        path, fmt, w, h, n = synth_yuv_444p8
        p = YUVParser(path, fmt, w, h)
        frame = p.read_frame(0)
        assert frame.u.shape == (h, w)
        assert frame.v.shape == (h, w)

    def test_mmap_path_exercised(self, tmp_path, rng):
        path = tmp_path / "big.yuv"
        _write_synthetic_yuv(path, "YUV420P8", 1920, 1080, 5, rng)
        p = YUVParser(str(path), "YUV420P8", 1920, 1080)
        assert p._mmap is not None, "Expected mmap to be used for >1MB file"
        frame = p.read_frame(2)
        assert frame.y.shape == (1080, 1920)
        p.close()

    def test_truncated_file_raises_with_index(self, tmp_path, rng):
        path = tmp_path / "trunc.yuv"
        _write_synthetic_yuv(path, "YUV420P8", 32, 16, 3, rng)
        size = path.stat().st_size
        path.write_bytes(path.read_bytes()[: size - 100])
        p = YUVParser(str(path), "YUV420P8", 32, 16)
        with pytest.raises(IndexError, match="out of range"):
            p.read_frame(p.num_frames)
