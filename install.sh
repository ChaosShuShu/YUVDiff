#!/usr/bin/env bash
set -e

# ==============================================================================
# YUVDiff One-Click Installation & System Deployment Script
# ==============================================================================

COLOR_GREEN="\033[0;32m"
COLOR_BLUE="\033[0;34m"
COLOR_YELLOW="\033[1;33m"
COLOR_RED="\033[0;31m"
COLOR_RESET="\033[0m"

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

PREFIX="/usr/local"
USER_INSTALL=false

for arg in "$@"; do
    case $arg in
        --prefix=*)
            PREFIX="${arg#*=}"
            shift
            ;;
        --user)
            USER_INSTALL=true
            PREFIX="${HOME}/.local"
            shift
            ;;
        -h|--help)
            echo "Usage: ./install.sh [options]"
            echo "Options:"
            echo "  --prefix=<path>   Installation prefix (default: /usr/local)"
            echo "  --user            Install for current user only (${HOME}/.local)"
            echo "  -h, --help        Show this help message"
            exit 0
            ;;
    esac
done

if [ "$USER_INSTALL" = false ] && [ "$EUID" -ne 0 ] && [ "$PREFIX" = "/usr/local" ]; then
    echo -e "${COLOR_YELLOW}Notice: Installing to /usr/local requires root permissions.${COLOR_RESET}"
    echo -e "You can either:"
    echo -e "  1. Run with sudo: ${COLOR_GREEN}sudo ./install.sh${COLOR_RESET}"
    echo -e "  2. Install for user only: ${COLOR_GREEN}./install.sh --user${COLOR_RESET}"
    echo -e "  3. Specify custom prefix: ${COLOR_GREEN}./install.sh --prefix=/path/to/dir${COLOR_RESET}\n"
fi

# 1. Build if not already built
if [ ! -f "${BUILD_DIR}/yuvdiff-cli" ]; then
    echo -e "${COLOR_BLUE}Building YUVDiff prior to installation...${COLOR_RESET}"
    "${PROJECT_ROOT}/build.sh"
fi

# 2. Install binaries and headers via CMake
echo -e "\n${COLOR_GREEN}Installing YUVDiff to ${PREFIX}...${COLOR_RESET}"
cmake --install "${BUILD_DIR}" --prefix "${PREFIX}"

# 3. Create desktop launcher if GUI is built
if [ -f "${PREFIX}/bin/yuvdiff-gui" ] || [ -f "${BUILD_DIR}/yuvdiff-gui" ]; then
    DESKTOP_DIR=""
    if [ "$PREFIX" = "/usr/local" ] || [ "$PREFIX" = "/usr" ]; then
        DESKTOP_DIR="/usr/share/applications"
    else
        DESKTOP_DIR="${HOME}/.local/share/applications"
    fi

    if [ -d "$DESKTOP_DIR" ] || mkdir -p "$DESKTOP_DIR" 2>/dev/null; then
        echo -e "${COLOR_BLUE}Creating desktop entry in ${DESKTOP_DIR}/yuvdiff.desktop...${COLOR_RESET}"
        cat <<EOF > "${DESKTOP_DIR}/yuvdiff.desktop"
[Desktop Entry]
Version=1.0
Type=Application
Name=YUVDiff
GenericName=YUV Video Analysis Tool
Comment=Professional pixel-level YUV video difference and quality analysis tool
Exec=${PREFIX}/bin/yuvdiff-gui
Icon=video-x-generic
Terminal=false
Categories=AudioVideo;Video;Development;
Keywords=YUV;Video;Quality;PSNR;SSIM;Diff;
EOF
        chmod +x "${DESKTOP_DIR}/yuvdiff.desktop" 2>/dev/null || true
    fi
fi

echo -e "\n${COLOR_GREEN}======================================================${COLOR_RESET}"
echo -e "${COLOR_GREEN}             Installation Successful!                 ${COLOR_RESET}"
echo -e "${COLOR_GREEN}======================================================${COLOR_RESET}"
echo -e "Installed binaries:"
echo -e "  - ${PREFIX}/bin/yuvdiff-cli"
if [ -f "${PREFIX}/bin/yuvdiff-gui" ]; then
    echo -e "  - ${PREFIX}/bin/yuvdiff-gui"
fi
echo -e "\nYou can now run ${COLOR_YELLOW}yuvdiff-cli --help${COLOR_RESET} or ${COLOR_YELLOW}yuvdiff-gui${COLOR_RESET} directly from anywhere!"
