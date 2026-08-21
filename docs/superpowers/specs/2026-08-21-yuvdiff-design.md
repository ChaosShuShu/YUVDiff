# YUVdiff Design Spec

**Date**: 2026-08-21
**Status**: Draft (pending user review)
**Author**: Brainstorming session with Mavis

---

## 1. Background & Goals

### 1.1 Problem

Video codec engineers need to compare two raw YUV video sequences at the
pixel level to debug encoding pipelines, evaluate codec quality, and verify
that two implementations produce equivalent output. Existing tools are either
too heavy (full encoder frameworks), too limited (CLI-only, no visual
inspection), or not focused on raw YUV (most assume H.264/265 containers).

### 1.2 Goals

- Pixel-level diff between two YUV video files (A vs B)
- Quantitative metrics: PSNR (Y/U/V + total) and SSIM (Y)
- Visualize diff directly in a GUI (heat map, threshold mask)
- Step through frames and auto-play
- Export the current visualization as PNG
- CLI mode for batch processing and CI integration

### 1.3 Non-Goals (v1)

- Decoding H.264/HEVC/AV1 containers (only raw YUV)
- Auto-resizing between mismatched resolutions
- More than 2 inputs at once
- Cross-platform binary packaging
- Custom themes or advanced UI styling
- Report export to file (deferred to v2) — v1 CLI prints CSV to stdout
  only; users can redirect to a file with shell `>` if needed
- Multiple PSNR weighting schemes (v1 uses the standard sample-count-
  weighted MSE-combined total PSNR)
- Multi-threshold UI (single fixed threshold in v1)

---

## 2. Target User & Use Case

**Primary user**: A video codec engineer (the author) on Linux.

**Secondary users**: A small team of 4-5 codec engineers sharing the same tool
internally.

**Typical workflows**:
1. Encode a source YUV with two codec settings; open both outputs in YUVdiff
   and step through frames to spot where they diverge.
2. Compare a CPU reference decoder output against a hardware-accelerated
   decoder output to find pipeline bugs.
3. Run the CLI in CI and pipe its CSV stdout into a shell-side check
   (e.g. `awk`) to assert that two builds of an encoder produce
   pixel-equivalent output within a defined threshold.

**Environment**: Linux primary. The user knows C/C++ and Python; no
front-end/JavaScript experience, so the GUI must be a native desktop app
(PySide6), not a web app.

---

## 3. Scope (v1)

### 3.1 In Scope

- **YUV formats**: `YUV420P`, `YUV422P`, `YUV444P` × `8bit` / `10bitLE` —
  6 format combinations total, no FFmpeg dependency (custom parser).
- **2-way diff**: One reference input (A) and one test input (B).
- **Pixel-level absolute diff**: per-channel (Y, U, V) absolute difference.
- **PSNR**: per-channel PSNR + MSE-combined total PSNR. The total is
  computed by combining per-channel MSEs (weighted by sample count) into
  a single MSE, then converting to PSNR — **not** by averaging the
  per-channel PSNRs (PSNR is not linear in MSE, so averaging PSNRs would
  be wrong). The sample-count weighting per format:
  - YUV420: `MSE_total = (4*MSE_Y + MSE_U + MSE_V) / 6`
  - YUV422: `MSE_total = (2*MSE_Y + MSE_U + MSE_V) / 4`
  - YUV444: `MSE_total = (MSE_Y + MSE_U + MSE_V) / 3`
  - Then `PSNR_total = 10 * log10(MAX^2 / MSE_total)`.
- **SSIM**: Y channel only, 11×11 sliding window, constants
  `C1 = (0.01 * MAX)^2`, `C2 = (0.03 * MAX)^2` where `MAX` is 255 (8bit)
  or 1023 (10bit).
- **Visualization modes** (radio-button selectable, hotkeys 1/2/3/4):
  - `Original A` — show frame A
  - `Original B` — show frame B
  - `Heatmap` — diff intensity → color gradient
  - `Threshold mask` — original A with red overlay on pixels above threshold
- **Frame navigation**:
  - Slider + spin box to jump to a specific frame
  - `←` / `→` keys to step one frame
  - `Space` to toggle auto-play (default 25 fps, configurable)
  - Auto-play loops back to frame 0 at the end by default
- **Export diff image**: Save the current frame's visualization as PNG
  (single frame or all frames as `frame_00000.png` etc.).
- **GUI** (PySide6) + **CLI** (argparse).

### 3.2 Out of Scope (deferred to v2 or later)

- Resolution auto-resize between mismatched A and B
- N-way (3+) comparison
- Decoding compressed video containers
- Cross-platform packaging (Windows / macOS installers)
- Report export (CSV / JSON) — CLI prints to stdout only in v1
- PSNR weighting toggle (BT.601 / BT.709 / simple average)
- Multi-threshold UI (single fixed threshold in v1)
- Per-channel threshold
- SSIM on U/V channels
- GUI test automation (no pytest-qt in v1)

---

## 4. Architecture

Five layers, strictly decoupled so that the core (parser, diff, metrics) can
be unit-tested without any GUI dependency.

```
┌────────────────────────────────────────┐
│  GUI (PySide6)  /  CLI (argparse)      │  ← User interaction
├────────────────────────────────────────┤
│  Renderer (Qt + QOpenGLWidget)         │  ← YUV→RGB, heatmap, mask
├────────────────────────────────────────┤
│  DiffEngine + MetricsCalculator        │  ← Pixel diff, PSNR, SSIM
├────────────────────────────────────────┤
│  YUVParser (format detect + read)      │  ← Frame-level reader
├────────────────────────────────────────┤
│  File I/O (mmap, lazy load)            │  ← Raw bytes
└────────────────────────────────────────┘
```

### 4.1 Key principles

- `parser`, `diff`, `metrics`, `formats` modules have **zero Qt imports**;
  `import yuvdiff.parser` must work in a headless environment.
- `DiffEngine` and `MetricsCalculator` are **stateless pure functions** of
  two `YUVFrame` objects.
- All inter-layer data exchange uses `np.ndarray` (no Qt types below the
  renderer).
- Frame data is passed by reference; the parser uses `mmap` so loading a
  single frame does not require reading the whole file.

---

## 5. Tech Stack

| Component | Choice | Reason |
|-----------|--------|--------|
| Language | Python 3.11+ | User is fluent; ecosystem is mature |
| GUI | PySide6 (LGPL) | Cross-platform, full Qt6, commercially friendly |
| Array | numpy | Required for any non-trivial pixel ops |
| Rendering | Qt's `QOpenGLWidget` (built-in) | No third-party dependency |
| SSIM | numpy + `scipy.ndimage.uniform_filter` | One kernel call per statistic |
| Test | pytest + tmp_path fixtures | Industry standard, no real YUV files in repo |
| Build | `pyproject.toml` + `pip install -e .` | Reproducible, simple |

### 5.1 Project layout

```
yuvdiff/
├── pyproject.toml
├── README.md
├── .gitignore
├── src/yuvdiff/
│   ├── __init__.py
│   ├── formats.py        # PixelFormat, BitDepth, parse_format()
│   ├── parser.py         # YUVParser with mmap + lazy frame read
│   ├── diff.py           # DiffEngine (absolute diff + threshold mask)
│   ├── metrics.py        # MetricsCalculator (PSNR + SSIM)
│   ├── renderer.py       # RenderMode enum + Renderer (YUV→RGB)
│   ├── gui.py            # PySide6 main window
│   └── cli.py            # argparse entry point
├── tests/
│   ├── conftest.py       # Synthetic YUV fixtures via np.random
│   ├── test_formats.py
│   ├── test_parser.py
│   ├── test_diff.py
│   ├── test_metrics.py
│   └── test_cli.py
└── docs/
    └── superpowers/
        └── specs/
            └── 2026-08-21-yuvdiff-design.md
```

---

## 6. Components

### 6.1 `formats.py` — Format definitions

```python
class PixelFormat(Enum):
    YUV420P = "YUV420P"
    YUV422P = "YUV422P"
    YUV444P = "YUV444P"

class BitDepth(Enum):
    BIT8 = 8
    BIT10LE = 10  # 16-bit LE storage, 10-bit payload

def parse_format(s: str) -> tuple[PixelFormat, BitDepth]:
    """Parse strings like 'YUV420P8', 'YUV420P10LE'. Raise on unsupported."""

def chroma_subsampling(fmt: PixelFormat) -> tuple[int, int]:
    """Return (horizontal, vertical) chroma sub-sampling factors."""
    # 420 -> (2, 2); 422 -> (2, 1); 444 -> (1, 1)

def frame_bytes(fmt: PixelFormat, w: int, h: int, bit_depth: int) -> int:
    """Single-frame byte size for the given format/resolution/bit depth."""
```

### 6.2 `parser.py` — YUVParser

```python
@dataclass
class YUVFrame:
    y: np.ndarray   # (H, W)        uint8 or uint16
    u: np.ndarray   # (H/sv, W/sh)
    v: np.ndarray
    bit_depth: int
    width: int
    height: int
    pixel_format: PixelFormat

class YUVParser:
    def __init__(self, path: str, fmt: str, width: int, height: int): ...
    @property
    def num_frames(self) -> int: ...   # file_size // frame_bytes
    def read_frame(self, idx: int) -> YUVFrame: ...
```

**Behavior**:
- Use `mmap` for files > 1 MB; small files load directly.
- `read_frame` is O(frame_bytes) — does not scan the whole file.
- 10-bit samples are read as `uint16` LE using the **MSB-aligned**
  convention (the value occupies bits [15:6]; the low 6 bits are
  padding). This matches H.264/HEVC/AV1 reference software. The actual
  10-bit value is recovered as `word >> 6` and stored as a 16-bit
  integer in the range [0, 1023].
- `num_frames` reflects the on-disk frame count; if the file is truncated,
  `read_frame` raises `ValueError` with a clear message naming the frame.

### 6.3 `diff.py` — DiffEngine

```python
@dataclass
class DiffResult:
    diff_y: np.ndarray   # (H, W)        absolute diff
    diff_u: np.ndarray
    diff_v: np.ndarray
    mask: np.ndarray     # (H, W) bool   True where any channel > threshold
    diff_pixel_count: int
    total_pixel_count: int

class DiffEngine:
    def __init__(self, threshold: int = 4): ...
    def diff(self, a: YUVFrame, b: YUVFrame) -> DiffResult: ...
```

**Behavior**:
- Threshold applies uniformly to all three channels in v1.
- `mask = (diff_y > t) | (diff_u_up > t) | (diff_v_up > t)` where U/V are
  nearest-neighbor upsampled to Y's resolution before the OR.
- `diff_*` arrays are returned at their native (possibly sub-sampled) shape;
  the renderer is responsible for upsampling when drawing the heatmap.

### 6.4 `metrics.py` — MetricsCalculator

```python
@dataclass
class PSNRResult:
    y: float
    u: float
    v: float
    total: float        # resolution-weighted per spec §3.1

@dataclass
class SSIMResult:
    y: float            # mean SSIM over the frame, range [-1, 1]
    # v1: U/V SSIM not implemented

class MetricsCalculator:
    def psnr(self, a: YUVFrame, b: YUVFrame) -> PSNRResult: ...
    def ssim(self, a: YUVFrame, b: YUVFrame) -> SSIMResult: ...
```

**Behavior**:
- `MAX` = 255 for 8-bit, 1023 for 10-bit.
- If `MSE == 0`, return `PSNR = inf` (sentinel float).
- SSIM uses an 11×11 box filter via `scipy.ndimage.uniform_filter` for
  local mean and variance; cross-variance is `E[AB] - E[A]*E[B]`.

### 6.5 `renderer.py` — Renderer

```python
class RenderMode(Enum):
    ORIGINAL_A = "a"
    ORIGINAL_B = "b"
    HEATMAP = "heatmap"
    THRESHOLD_MASK = "mask"

class Renderer:
    def __init__(self, width: int, height: int): ...
    def render(self,
               frame_a: YUVFrame,
               frame_b: YUVFrame | None,   # None for modes that don't need B
               diff: DiffResult | None,    # None for A/B modes
               mode: RenderMode,
               threshold: int) -> QImage:  # RGBA8888
```

**Behavior**:
- YUV → RGB uses the BT.601 limited-range matrix (the most common for
  8-bit video content).
- For 10-bit input, the value is already in [0, 1023] (see §6.2). The
  renderer scales it to 8-bit for display via `value >> 2` (giving
  [0, 255]) before applying the same matrix; this is a deliberate v1
  simplification (full 10-bit RGB output is not in scope).
- `HEATMAP` uses a "blue → green → yellow → red" gradient keyed to
  `diff / MAX`.
- `THRESHOLD_MASK` overlays red (`#FF0000` at 60% alpha) on original A
  wherever `mask` is True.

### 6.6 `gui.py` — GUI

```
┌──────────────────────────────────────────────────┐
│ [Open A] [Open B] [Format ▾] [W:] [H:]          │
│ [Threshold ━●━] [Mode ▾] [Export ▾] [▶ Play]   │
├────────────┬─────────────────────────────────────┤
│  Frame:    │                                     │
│  ━━●━━ 42  │                                     │
│  [◀][▶]    │       Visualization canvas          │
│  25 fps    │                                     │
│            │                                     │
├────────────┴─────────────────────────────────────┤
│ Frame 42/300 │ PSNR Y=38.2 U=42.1 V=43.5 T=39.0  │
│ SSIM Y=0.987 │ Diff: 1.2% (12453/2073600)        │
└──────────────────────────────────────────────────┘
```

**Hotkeys**:
- `←` / `→` — previous / next frame
- `Space` — toggle auto-play
- `1` / `2` / `3` / `4` — switch render mode

**Threading**:
- All disk I/O and diff/metrics run on a `QThreadPool` worker when frame
  resolution > 4K, to avoid blocking the UI. Smaller frames compute inline.
- Results are posted back to the main thread via `Signal`.

### 6.7 `cli.py` — CLI

```bash
yuvdiff a.yuv b.yuv \
  --format YUV420P8 \
  --width 1920 --height 1080 \
  --frames 100 \
  --threshold 4
# → prints CSV to stdout; redirect with `> report.csv` if needed
```

- Required args: both file paths, `--format`, `--width`, `--height`.
- Optional: `--frames` (default: min of both files' frame counts),
  `--threshold` (default: 4).
- Exit codes: 0 success, 1 format / arg error, 2 file I/O error.
- Pass/fail assertion is the caller's responsibility (e.g. shell-side
  `awk` over the CSV) — no built-in threshold gate in v1.

---

## 7. Data Flow

### 7.1 GUI single-frame render

```
User moves slider to frame 42
   ↓
Controller emits show_frame(42)
   ↓
   ├── parser_a.read_frame(42)        # mmap + numpy.frombuffer
   ├── parser_b.read_frame(42)
   ├── diff_engine.diff(a, b)         # absolute diff + mask
   ├── metrics_calc.psnr(a, b)        # cached if frame unchanged
   ├── metrics_calc.ssim(a, b)        # cached if frame unchanged
   ├── renderer.render(...)           # YUV→RGB + mode composite
   ↓
canvas.setImage(qimage)
   ↓
status bar updated with PSNR / SSIM / diff count
```

### 7.2 Auto-play

```
QTimer.start(1000 / fps)
   ↓ each tick
   current_frame = (current_frame + 1) % num_frames
   → 7.1 with new frame index
```

### 7.3 Export diff image

- **Current frame**: render the current mode → save as
  `<output_dir>/<a_basename>_vs_<b_basename>_frame_{idx:05d}.png`.
- **All frames**: loop with progress bar; same naming pattern.

### 7.4 CLI flow

```
argparse parses args
   ↓
open both YUVParser instances
   ↓
for i in range(frames):
   read_frame(i) × 2
   compute PSNR + SSIM + diff count
   emit one CSV line to stdout
   ↓
exit 0
```

---

## 8. Error Handling

| Scenario | Handling |
|----------|----------|
| Unsupported format string | Fail at startup with the list of supported formats |
| Resolution mismatch (A.W ≠ B.W or A.H ≠ B.H) | Fail at startup; no auto-resize in v1 |
| File shorter than expected for N frames | Raise `ValueError` naming the truncated frame index |
| Frame index out of bounds | Clamp the slider; emit a status-bar warning |
| MemoryError on huge frame | Catch, show a dialog suggesting smaller resolution or more RAM |
| 10-bit file with declared 8-bit format | Raise immediately on first frame read (sanity check on a few sample values) |

---

## 9. Testing Strategy

### 9.1 Layered tests

| Layer | Tool | Coverage |
|-------|------|----------|
| Unit | pytest | All pure-function modules |
| Integration | pytest | CLI end-to-end (small fixture → assert stdout/exit code) |
| Visual regression | _none in v1_ | Skip until a need appears |

### 9.2 Fixture strategy

No real YUV files in the repo. `tests/conftest.py` exposes fixtures that
generate tiny synthetic YUVs (e.g. 32×16, 5 frames, random data) using
`np.random.randint`. One fixture per format (6 total) plus convenience
fixtures for two-frame-diff scenarios.

### 9.3 Required test cases (one test each)

**`test_formats.py`**
- `parse_format("YUV420P8")` → `(YUV420P, BIT8)`
- `parse_format("YUV420P10LE")` → `(YUV420P, BIT10LE)`
- `parse_format("YUV420P10BE")` → raises
- `chroma_subsampling(YUV420P)` → `(2, 2)`

**`test_parser.py`**
- Each of the 6 formats: `read_frame(0)` returns correct Y/U/V shapes and dtypes
- 10-bit Y values lie in [0, 1023] (verifies the MSB-aligned `>> 6`
  extraction is applied)
- Truncated file → `ValueError` mentioning the frame index
- mmap path is exercised for a fixture > 1 MB

**`test_diff.py`**
- Identical frames → all-zero diff, empty mask
- Black vs white (8-bit) → diff == 255, mask all True
- Threshold boundary: diff == threshold does not trigger the mask
- Per-channel independence: diffing only Y still produces U/V zero

**`test_metrics.py`**
- Identical frames → `PSNR = inf`, `SSIM = 1.0`
- Hand-computed PSNR for a known synthetic pair matches `MetricsCalculator.psnr`
- 8-bit uses `MAX = 255`, 10-bit uses `MAX = 1023`
- Weighted total formula uses the correct ratio per format by combining
  MSEs (4:1:1 for 420, 2:1:1 for 422, 1:1:1 for 444) before the
  `10*log10` step, **not** by averaging per-channel PSNRs

**`test_cli.py`**
- No args → non-zero exit, usage printed
- Two valid synthetic files → CSV output with one row per frame
- Format mismatch → exit 1, clear error message

---

## 10. Performance Targets

Targets at 1080p (1920×1080), YUV420P8, on a modern Linux workstation.
4K roughly ×4; 8K roughly ×16.

| Operation | Target |
|-----------|--------|
| `parser.read_frame` (mmap) | < 5 ms |
| `DiffEngine.diff` | < 30 ms |
| `MetricsCalculator.psnr` | < 30 ms |
| `MetricsCalculator.ssim` | < 100 ms |
| `Renderer.render` (incl. YUV→RGB) | < 50 ms |
| **Total: slider drag → new image on screen** | **< 200 ms** |
| Auto-play at 25 fps | Stable, no dropped frames |

### 10.1 Fallback if performance is missed

In order of effort vs. payoff:
1. JIT SSIM with Numba (1-2 hours work, ~3× speedup)
2. Move diff/metrics off the UI thread for > 4K frames (already in design)
3. Rewrite the hot path in Cython (last resort, only if needed)

Not pursued in v1; the above are the contingency plan.

---

## 11. Open Questions

None at design time. All major forks were resolved during brainstorming
(see §3 for the explicit v1/v2 split).

---

## 12. Approval

- [ ] User reviewed the spec on `2026-08-21`
- [ ] Spec committed to git
- [ ] Transitioned to `writing-plans` skill
