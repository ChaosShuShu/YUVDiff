#!/usr/bin/env bash
set -e

# ==============================================================================
# YUVDiff One-Click Uninstallation & Clean-Up Script
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
            echo "Usage: ./uninstall.sh [options]"
            echo "Options:"
            echo "  --prefix=<path>   Installation prefix to clean (default: /usr/local)"
            echo "  --user            Uninstall from current user only (${HOME}/.local)"
            echo "  -h, --help        Show this help message"
            exit 0
            ;;
    esac
done

if [ "$USER_INSTALL" = false ] && [ "$EUID" -ne 0 ] && [ "$PREFIX" = "/usr/local" ]; then
    echo -e "${COLOR_YELLOW}Notice: Uninstalling from /usr/local may require root permissions.${COLOR_RESET}"
    echo -e "You can either:"
    echo -e "  1. Run with sudo: ${COLOR_GREEN}sudo ./uninstall.sh${COLOR_RESET}"
    echo -e "  2. Uninstall from user only: ${COLOR_GREEN}./uninstall.sh --user${COLOR_RESET}"
    echo -e "  3. Specify custom prefix: ${COLOR_GREEN}./uninstall.sh --prefix=/path/to/dir${COLOR_RESET}\n"
fi

# 1. Run CMake target uninstall if build directory exists
if [ -d "${BUILD_DIR}" ]; then
    echo -e "${COLOR_BLUE}Running CMake uninstall target...${COLOR_RESET}"
    cmake --build "${BUILD_DIR}" --target uninstall 2>/dev/null || true
fi

# 2. Clean up prefix-specific binaries and libraries if manifest didn't catch them
echo -e "\n${COLOR_BLUE}Checking installed files under ${PREFIX}...${COLOR_RESET}"
rm -f "${PREFIX}/bin/yuvdiff-cli" 2>/dev/null || true
rm -f "${PREFIX}/bin/yuvdiff-gui" 2>/dev/null || true
rm -f "${PREFIX}/lib/libyuvdiff_core.a" 2>/dev/null || true
rm -rf "${PREFIX}/include/yuvdiff" 2>/dev/null || true

# 3. Clean up desktop launchers
DESKTOP_FILES=(
    "/usr/share/applications/yuvdiff.desktop"
    "${HOME}/.local/share/applications/yuvdiff.desktop"
    "${PREFIX}/share/applications/yuvdiff.desktop"
)

for dfile in "${DESKTOP_FILES[@]}"; do
    if [ -f "$dfile" ]; then
        echo -e "${COLOR_BLUE}Removing desktop entry: ${dfile}${COLOR_RESET}"
        rm -f "$dfile" 2>/dev/null || true
    fi
done

echo -e "\n${COLOR_GREEN}======================================================${COLOR_RESET}"
echo -e "${COLOR_GREEN}            Uninstallation Completed!                 ${COLOR_RESET}"
echo -e "${COLOR_GREEN}======================================================${COLOR_RESET}"
echo -e "YUVDiff has been successfully removed from your system.\n"
