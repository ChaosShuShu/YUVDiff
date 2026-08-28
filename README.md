# YUVDiff

**YUVDiff** 是一个面向工业级视频编解码与画质评测的现代高性能 **YUV 视频像素级深度比对与客观质量分析工具**。

支持 **8-bit / 10-bit**、**4:2:0 / 4:2:2 / 4:4:4** 任意格式跨采样比对，采用 **C++17 + GPU 片元着色器硬件加速 + CPU AVX2 SIMD 向量化加速**，兼具 **60+ FPS 丝滑播放的 GUI 可视化工作台** 与 **全能高效的自动化 CLI 命令行工具箱**。

---

## ✨ 核心特性矩阵

### 1. 🖥️ 现代化 GPU 可视化工作台 (`yuvdiff-gui`)
- **硬件级 GPU 渲染**：Qt6 + OpenGL 片元着色器实时色彩转换（BT.601），60+ FPS 双视频同步丝滑播放；
- **微观像素级检验（YUView 风格）**：
  - **物理像素网格**：放大至像素级自动呈现像素物理边界；
  - **原位 YUV 数值彩色标注**：在每个像素格内直观显示 $Y$、$U$、$V$ 或差值 $\Delta$ 数值；
  - **光标像素探测器（Cursor Inspector）**：鼠标滑动毫秒级实时探测光标处像素坐标、两路 Y/U/V 原始采样及误差；
  - **平滑缩放与平移**：支持鼠标滚轮 `0.1x ~ 80x` 自由缩放与按住中键/拖拽平移；
- **多维可视化渲染模态**：
  - `Original A` / `Original B`：单路原始视频无损预览；
  - `Heatmap`（动态热力图）：0 误差中性灰 $\to$ 最大误差纯红热力渐变；
  - `Threshold Mask`（阈值掩模）：原图 A 叠加超阈值误差像素红色高亮；
  - `Overlay`：A/B 画面半透明融合对比；
- **跨格式与跨位深独立控制**：Video A 与 Video B 独立格式解析与自动探测，杜绝冲突。

---

### 2. ⚡ 高性能自动化 CLI 命令行工具箱 (`yuvdiff-cli`)
- **`diff`（统一画质比对与指标度量）**：
  - 全通道客观度量：$\text{PSNR}_Y, \text{PSNR}_U, \text{PSNR}_V, \text{PSNR}_{Total}$ 及 $O(1)$ 均值滑窗 $\text{SSIM}_Y$；
  - 全套直方图统计：`diff_pixels`（差异像素数）、`diff_ratio`（占比）、`diff_mean`（均值）、`diff_median`（中位数）、`diff_max`、`diff_min`；
  - `--stop-on-diff`：遇到首个差异帧立即中断并输出定位信息；
  - `-q, --quiet`：静默模式（纯返回码 `0`（一致）/ `1`（差异），专为 CI/CD 流水线设计）；
- **`cmp`（结构化对比模式）**：
  - `-o, --output <file>`：将全序列逐帧统计指标以 `K=V` 格式写入文件；
  - `stdout` 输出整段视频汇总总结性统计数据（Summary stats）；
- **`cut`（极速 YUV 视频切片提取器）**：
  - 支持按起始帧与帧数（`-s`, `-n`）毫秒级截取保存新 YUV 视频片段；
- **`info`（视频属性与 10-bit 对齐探测）**：
  - 快速查看视频分辨率、格式、位深、总帧数及 MSB/LSB 自动对齐探测。

---

### 3. 🚀 极致性能架构
- **CPU SIMD 向量化加速（AVX2 / SSE4.1）**：单周期并发处理 32/16 字节，纯 CPU 度量吞吐提升 **3x ~ 6x**；
- **零拷贝内存映射（`mmap`）**：支持 GB 级大视频秒级加载与低内存占用；
- **LRU 缓存与双缓冲预取**：播放与拖动进度条无缝响应。

---

## 📦 支持的 YUV 格式

| 格式标识 | 色度采样比 | 采样位数 | 存储规范 |
| :--- | :--- | :--- | :--- |
| **YUV420P8** | 4:2:0 | 8-bit | Planar (Y, U, V) |
| **YUV422P8** | 4:2:2 | 8-bit | Planar (Y, U, V) |
| **YUV444P8** | 4:4:4 | 8-bit | Planar (Y, U, V) |
| **YUV420P10LE** | 4:2:0 | 10-bit | 16-bit LE (支持 MSB / LSB 自动对齐) |
| **YUV422P10LE** | 4:2:2 | 10-bit | 16-bit LE (支持 MSB / LSB 自动对齐) |
| **YUV444P10LE** | 4:4:4 | 10-bit | 16-bit LE (支持 MSB / LSB 自动对齐) |

> 💡 **跨格式比对支持**：支持任意 8-bit 与 10-bit 混合比对（自动对齐 $\times 4$），以及 4:2:0、4:2:2、4:4:4 混合比对（自动色度无损上采样度量）。

---

## 🛠️ 环境依赖与快速安装

### 1. 安装系统依赖

#### Ubuntu / Debian / Deepin / Mint
```bash
sudo apt update
sudo apt install -y cmake build-essential libqt6openglwidgets6-dev qt6-base-dev
```

#### Fedora / RHEL / CentOS Stream
```bash
sudo dnf install -y cmake gcc-c++ qt6-qtbase-devel
```

#### Arch Linux / Manjaro
```bash
sudo pacman -S cmake base-devel qt6-base
```

---

### 2. 编译构建指南

#### 场景 A：本地完整构建（CLI + GUI 工作台）
```bash
./build.sh
```

#### 场景 B：Linux 服务器/无头环境纯 CLI 构建（无需任何 Qt6/OpenGL 依赖）
适合在云服务器、无桌面 Linux 服务器、Docker 容器或 CI/CD 构建机中直接编译：
```bash
./build.sh --cli-only
```
*(仅需基础环境 `cmake` 与 `g++` 即可秒级编译成功)*

#### 场景 C：打包纯静态独立可执行文件（可在任意 Linux 服务器直接运行）
```bash
./build.sh --static
```
*(生成无动态库依赖的独立二进制，复制到任意 Linux 服务器均可直接运行)*

---

### 3. 一键安装部署到系统

#### 方式 A：系统全局安装（推荐）
```bash
sudo ./install.sh
```
安装完成后，可在终端任意位置直接输入 `yuvdiff-cli` 或 `yuvdiff-gui`，并在 Linux 桌面应用菜单中直接点击 **YUVDiff** 图标启动！

#### 方式 B：当前用户安装（无需 root 权限）
```bash
./install.sh --user
```
将安装至 `~/.local/bin`，并创建用户级桌面快捷方式。

---

## 📖 快速上手指南

### 1. 🖥️ GUI 图形界面使用

```bash
# 方式 1：直接启动工作台
yuvdiff-gui

# 方式 2：启动并直接加载对比两路视频
yuvdiff-gui video_a.yuv video_b.yuv
```

#### 常用快捷键与操作：
- **`Space`**：播放 / 暂停视频；
- **`←` / `→`**：后退 / 前进单帧；
- **`1` / `2` / `3` / `4` / `5`**：快速切换视图模式（`Original A` / `Original B` / `Heatmap` / `Threshold Mask` / `Overlay`）；
- **鼠标滚轮**：以光标为中心进行平滑缩放（放大后自动激活像素网格与数值标注）；
- **鼠标左键/中键拖拽**：平移画面画布；
- **鼠标滑动**：状态栏左侧实时显示光标所在像素的坐标、A/B 采样值及误差 $\Delta$。

---

### 2. ⚡ CLI 命令行工具使用

#### 场景 1：逐帧对比画质并输出 CSV（`diff`）
```bash
yuvdiff-cli diff video_a.yuv video_b.yuv \
    --format-a YUV420P8 \
    --format-b YUV420P10LE \
    --width 1280 --height 720 \
    --frames 100 \
    --threshold 4
```
**输出示例**：
```text
frame,psnr_y,psnr_u,psnr_v,psnr_total,ssim_y,diff_pixels,total_pixels,diff_mean,diff_median,diff_max,diff_min
0,inf,52.8736,59.3065,59.7654,1.0000,47672,921600,2.0219,2.0000,11,0
1,inf,52.8736,59.3065,59.7654,1.0000,47672,921600,2.0219,2.0000,11,0
```

#### 场景 2：极速首帧差异定位与自动化静默校验（CI/CD）
```bash
# 遇到首个不同帧立即停止
yuvdiff-cli diff video_a.yuv video_b.yuv --stop-on-diff -w 1280 -h 720

# CI 静默模式（退出码 0 表示一致，1 表示差异）
if yuvdiff-cli diff video_a.yuv video_b.yuv -q; then
    echo "Videos are identical!"
else
    echo "Regression detected!"
fi
```

#### 场景 3：结构化统计与逐帧日志导出（`cmp`）
```bash
yuvdiff-cli cmp video_a.yuv video_b.yuv \
    -w 1280 -h 720 \
    -o /tmp/frame_stats.txt
```
- **`stdout` 汇总输出**：
  ```text
  total_frames=504 diff_frames=5 diff_pixels=1027614 total_pixels=4608000 diff_ratio=0.2230 diff_mean=3.2242 diff_median=0.0000 diff_max=37 diff_min=0
  ```
- **`/tmp/frame_stats.txt` 逐帧记录**：
  ```text
  frame=0 diff_pixels=47672 total_pixels=921600 diff_ratio=0.0517 diff_mean=2.0219 diff_median=2.0000 diff_max=11 diff_min=0
  frame=1 diff_pixels=47672 total_pixels=921600 diff_ratio=0.0517 diff_mean=2.0219 diff_median=2.0000 diff_max=11 diff_min=0
  ```

#### 场景 4：视频切片提取（`cut`）
从大视频中快速截取从第 10 帧开始的 50 帧并保存到新文件：
```bash
yuvdiff-cli cut -i raw_input.yuv -o sample_50frames.yuv -s 10 -n 50 -w 1920 -h 1080 -f YUV420P8
```

#### 场景 5：查看视频元信息（`info`）
```bash
yuvdiff-cli info /path/to/video_720p50_10le.yuv
```
**输出示例**：
```text
=== YUV Video Info ===
File: /path/to/video_720p50_10le.yuv
Format: YUV420P10LE
Resolution: 1280x720
Bit Depth: 10LE
Total Frames: 504
Alignment: lsb
```

---

## 🧪 自动化测试

运行全套 C++ 单元测试：
```bash
./build/yuvdiff_test
```
全部 **30 项测试用例 100% 通过**，涵盖格式解析、内存映射、SIMD 计算、PSNR/SSIM 算法精度、直方图统计及视频切片提取。

---

## 📄 License

MIT License.
