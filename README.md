# YUVdiff

Pixel-level diff for raw YUV video sequences. Compare two YUV files at
the pixel level, view the diff in a GUI, and dump per-frame metrics to
the CLI.

## Supported Formats

| Format         | Sub-sampling | Bit depth |
|----------------|--------------|-----------|
| YUV420P8       | 4:2:0        | 8         |
| YUV422P8       | 4:2:2        | 8         |
| YUV444P8       | 4:4:4        | 8         |
| YUV420P10LE    | 4:2:0        | 10 (MSB-aligned LE) |
| YUV422P10LE    | 4:2:2        | 10 (MSB-aligned LE) |
| YUV444P10LE    | 4:4:4        | 10 (MSB-aligned LE) |

10-bit samples are MSB-aligned in 16-bit LE words (H.264/HEVC/AV1
reference convention).

## Install

```bash
git clone <this repo>
cd yuvdiff
pip install -e ".[dev]"
```

Linux only in v1. Requires Python 3.11+.

## GUI

```bash
python -m yuvdiff.gui
```

Then:
1. Click **Open A…** and **Open B…** to load the two YUV files
2. Set the format, width, height in the toolbar
3. Use the slider or `←` / `→` to step through frames
4. Press `Space` to play / pause; `1/2/3/4` to switch view mode
5. Click **Export PNG** to save the current frame, **Export All…** to dump all

Visualization modes:
- **Original A** / **Original B** — show the raw frame
- **Heatmap** — diff intensity → color gradient
- **Threshold mask** — original A with red overlay on pixels above the threshold

## CLI

```bash
yuvdiff A.yuv B.yuv --format YUV420P8 --width 1920 --height 1080
```

Prints per-frame CSV to stdout. Redirect to a file:

```bash
yuvdiff A.yuv B.yuv --format YUV420P8 --width 1920 --height 1080 > metrics.csv
```

CSV columns:
`frame, psnr_y, psnr_u, psnr_v, psnr_total, ssim_y, diff_pixels, total_pixels`

Use shell tools to assert pass/fail:

```bash
yuvdiff A.yuv B.yuv --format YUV420P8 --width 1920 --height 1080 | \
  awk -F, 'NR>1 && $5 != "inf" && $5+0 < 35 { exit 1 }'
```

Exit codes: 0 success, 1 format/argument error, 2 file I/O error.

## Test

```bash
pytest
```

## Project Layout

```
src/yuvdiff/
  formats.py    # format enums + parsing
  parser.py     # mmap-backed YUV reader
  diff.py       # pixel-level absolute diff + threshold mask
  metrics.py    # PSNR + SSIM
  renderer.py   # 4 visualization modes -> QImage
  gui.py        # PySide6 main window
  cli.py        # argparse entry
tests/          # pytest suite with synthetic YUV fixtures
```

## v1 Limitations

- 2 inputs only (no N-way comparison)
- Same resolution required (no auto-resize)
- Linux only (Windows / macOS not packaged)
- Single threshold, uniform across channels
- PSNR total uses sample-count MSE weighting (not configurable)
- SSIM is Y-only (no U/V SSIM in v1)
- CLI prints CSV to stdout only (no JSON, no `--output` flag)
- No compressed-container decoding (raw YUV only)
- The GUI requires a working libEGL/EGL on the host; if unavailable,
  the CLI is the only usable interface.
