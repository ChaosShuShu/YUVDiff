import pytest

from yuvdiff.formats import BitDepth, PixelFormat, parse_format


class TestParseFormat:
    def test_8bit_formats(self):
        assert parse_format("YUV420P8") == (PixelFormat.YUV420P, BitDepth.BIT8)
        assert parse_format("YUV422P8") == (PixelFormat.YUV422P, BitDepth.BIT8)
        assert parse_format("YUV444P8") == (PixelFormat.YUV444P, BitDepth.BIT8)

    def test_10bit_le_formats(self):
        assert parse_format("YUV420P10LE") == (PixelFormat.YUV420P, BitDepth.BIT10LE)
        assert parse_format("YUV422P10LE") == (PixelFormat.YUV422P, BitDepth.BIT10LE)
        assert parse_format("YUV444P10LE") == (PixelFormat.YUV444P, BitDepth.BIT10LE)

    def test_10bit_be_rejected(self):
        with pytest.raises(ValueError, match="Big-endian 10-bit is not supported"):
            parse_format("YUV420P10BE")

    def test_unknown_format_rejected(self):
        with pytest.raises(ValueError, match="Unsupported format"):
            parse_format("YUV411P8")

    def test_garbage_rejected(self):
        with pytest.raises(ValueError, match="Unsupported format"):
            parse_format("nonsense")


from yuvdiff.formats import chroma_subsampling, frame_bytes


class TestChromaSubsampling:
    def test_420(self):
        assert chroma_subsampling(PixelFormat.YUV420P) == (2, 2)

    def test_422(self):
        assert chroma_subsampling(PixelFormat.YUV422P) == (2, 1)

    def test_444(self):
        assert chroma_subsampling(PixelFormat.YUV444P) == (1, 1)


class TestFrameBytes:
    def test_yuv420p8_1080p(self):
        # Y: 1920*1080 = 2_073_600; U+V: 2*960*540 = 1_036_800; total = 3_110_400
        assert frame_bytes(PixelFormat.YUV420P, 1920, 1080, BitDepth.BIT8) == 3_110_400

    def test_yuv422p8_1080p(self):
        # Y: 1920*1080; U+V: 2*960*1080 = 2_073_600; total = 4_147_200
        assert frame_bytes(PixelFormat.YUV422P, 1920, 1080, BitDepth.BIT8) == 4_147_200

    def test_yuv444p8_1080p(self):
        # Y+U+V all full res: 3*1920*1080 = 6_220_800
        assert frame_bytes(PixelFormat.YUV444P, 1920, 1080, BitDepth.BIT8) == 6_220_800

    def test_yuv420p10le_1080p(self):
        # 2 bytes per sample -> double the 8-bit 420 total
        assert frame_bytes(PixelFormat.YUV420P, 1920, 1080, BitDepth.BIT10LE) == 6_220_800

    def test_zero_resolution_rejected(self):
        with pytest.raises(ValueError, match="Resolution must be positive"):
            frame_bytes(PixelFormat.YUV420P, 0, 1080, BitDepth.BIT8)
