#!/usr/bin/env bash
set -e

# ==============================================================================
# YUVDiff One-Click Build Script
# ==============================================================================

COLOR_GREEN="\033[0;32m"
COLOR_BLUE="\033[0;34m"
COLOR_YELLOW="\033[1;33m"
COLOR_RED="\033[0;31m"
COLOR_RESET="\033[0m"

echo -e "${COLOR_BLUE}======================================================${COLOR_RESET}"
echo -e "${COLOR_BLUE}           YUVDiff High-Performance Build            ${COLOR_RESET}"
echo -e "${COLOR_BLUE}======================================================${COLOR_RESET}"

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
BIN_DIR="${PROJECT_ROOT}/bin"

CLI_ONLY=false
STATIC_BUILD=false
ENABLE_AVX2=ON

for arg in "$@"; do
    case $arg in
        --cli-only|-c)
            CLI_ONLY=true
            shift
            ;;
        --static)
            STATIC_BUILD=true
            CLI_ONLY=true
            shift
            ;;
        --no-avx2)
            ENABLE_AVX2=OFF
            shift
            ;;
        -h|--help)
            echo "Usage: ./build.sh [options]"
            echo "Options:"
            echo "  --cli-only, -c    Build headless CLI tool only (no Qt6/OpenGL required, ideal for servers)"
            echo "  --static          Build statically linked standalone CLI binary"
            echo "  --no-avx2         Disable AVX2 SIMD (for maximum compatibility with older CPUs)"
            echo "  -h, --help        Show this help message"
            exit 0
            ;;
    esac
done

# 1. Dependency Check
echo -e "\n${COLOR_GREEN}[1/4] Checking build dependencies...${COLOR_RESET}"

MISSING_DEPS=()
if ! command -v cmake &> /dev/null; then
    MISSING_DEPS+=("cmake")
fi
if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    MISSING_DEPS+=("g++ or clang++")
fi

if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo -e "${COLOR_RED}Error: Missing essential build dependencies:${COLOR_RESET} ${MISSING_DEPS[*]}"
    echo -e "Please install them via your package manager:"
    echo -e "  Ubuntu/Debian: ${COLOR_YELLOW}sudo apt install -y cmake build-essential${COLOR_RESET}"
    echo -e "  Fedora/RHEL:   ${COLOR_YELLOW}sudo dnf install -y cmake gcc-c++${COLOR_RESET}"
    echo -e "  Arch Linux:    ${COLOR_YELLOW}sudo pacman -S cmake base-devel${COLOR_RESET}"
    exit 1
fi

# Check Qt6 for GUI
if [ "$CLI_ONLY" = false ]; then
    HAS_QT6=false
    if pkg-config --exists Qt6Widgets Qt6OpenGLWidgets 2>/dev/null || command -v qmake6 &>/dev/null; then
        HAS_QT6=true
    fi
    if [ "$HAS_QT6" = false ]; then
        echo -e "${COLOR_YELLOW}[Notice] Qt6 development libraries not detected.${COLOR_RESET}"
        echo -e "If you want to build the graphical interface (yuvdiff-gui), please install:"
        echo -e "  Ubuntu/Debian: ${COLOR_YELLOW}sudo apt install -y qt6-base-dev libqt6openglwidgets6-dev${COLOR_RESET}"
        echo -e "  Fedora/RHEL:   ${COLOR_YELLOW}sudo dnf install -y qt6-qtbase-devel${COLOR_RESET}"
        echo -e "  Arch Linux:    ${COLOR_YELLOW}sudo pacman -S qt6-base${COLOR_RESET}"
        echo -e "Otherwise, CMake will automatically build the headless CLI tool only.\n"
    fi
fi

echo -e "Basic dependencies verified."

# 2. CMake Configuration
echo -e "\n${COLOR_GREEN}[2/4] Configuring project with CMake...${COLOR_RESET}"

CMAKE_FLAGS=("-DCMAKE_BUILD_TYPE=Release" "-DENABLE_AVX2=${ENABLE_AVX2}")

if [ "$CLI_ONLY" = true ]; then
    CMAKE_FLAGS+=("-DBUILD_GUI=OFF")
else
    CMAKE_FLAGS+=("-DBUILD_GUI=ON")
fi

if [ "$STATIC_BUILD" = true ]; then
    CMAKE_FLAGS+=("-DSTATIC_CLI=ON")
fi

cmake -B "${BUILD_DIR}" -S "${PROJECT_ROOT}" "${CMAKE_FLAGS[@]}"

# 3. Compilation
NPROC=$(nproc 2>/dev/null || echo 4)
echo -e "\n${COLOR_GREEN}[3/4] Compiling with ${NPROC} threads...${COLOR_RESET}"
cmake --build "${BUILD_DIR}" -j"${NPROC}"

# Create convenient bin/ directory
mkdir -p "${BIN_DIR}"
ln -sf "${BUILD_DIR}/yuvdiff-cli" "${BIN_DIR}/yuvdiff-cli"
if [ -f "${BUILD_DIR}/yuvdiff-gui" ]; then
    ln -sf "${BUILD_DIR}/yuvdiff-gui" "${BIN_DIR}/yuvdiff-gui"
fi

# 4. Automated Testing
echo -e "\n${COLOR_GREEN}[4/4] Running automated test suite...${COLOR_RESET}"
"${BUILD_DIR}/yuvdiff_test"

echo -e "\n${COLOR_GREEN}======================================================${COLOR_RESET}"
echo -e "${COLOR_GREEN}               Build Completed Successfully!          ${COLOR_RESET}"
echo -e "${COLOR_GREEN}======================================================${COLOR_RESET}"
echo -e "Build Artifacts in ${COLOR_YELLOW}${BIN_DIR}/${COLOR_RESET}:"
echo -e "  - CLI Tool: [OK] ${COLOR_YELLOW}${BIN_DIR}/yuvdiff-cli${COLOR_RESET}"
if [ -f "${BUILD_DIR}/yuvdiff-gui" ]; then
    echo -e "  - GUI App:  [OK] ${COLOR_YELLOW}${BIN_DIR}/yuvdiff-gui${COLOR_RESET}"
else
    echo -e "  - GUI App:  ${COLOR_YELLOW}[SKIPPED]${COLOR_RESET} (Install Qt6/Qt6OpenGLWidgets development packages to build GUI)"
fi

echo -e "\nQuick start examples:"
echo -e "  ${COLOR_BLUE}./bin/yuvdiff-cli --help${COLOR_RESET}"
if [ -f "${BUILD_DIR}/yuvdiff-gui" ]; then
    echo -e "  ${COLOR_BLUE}./bin/yuvdiff-gui${COLOR_RESET}"
fi
echo -e "  To install system-wide, run: ${COLOR_YELLOW}sudo ./install.sh${COLOR_RESET}"
