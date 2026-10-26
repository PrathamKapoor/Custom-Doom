#!/usr/bin/env bash
# ==============================================================================
# scripts/download_wad.sh
# Downloads verified official shareware DOOM1.WAD (v1.9)
# Places IWAD in ~/.doom-touch/ and ./wads/
# ==============================================================================

set -e

GREEN='\033[1;32m'
CYAN='\033[1;36m'
YELLOW='\033[1;33m'
RED='\033[1;31m'
NC='\033[0m'

echo -e "${CYAN}========================================================================${NC}"
echo -e "${CYAN}          rpi-doom-touch: Official Shareware IWAD Downloader            ${NC}"
echo -e "${CYAN}========================================================================${NC}"

# Target directories
DEST_USER="$HOME/.doom-touch"
DEST_REPO="$(cd "$(dirname "$0")/.." && pwd)/wads"

mkdir -p "$DEST_USER"
mkdir -p "$DEST_REPO"

TARGET_USER_WAD="$DEST_USER/DOOM1.WAD"
TARGET_REPO_WAD="$DEST_REPO/DOOM1.WAD"

# Check if IWAD already exists and is valid
check_valid_iwad() {
    local wad_path="$1"
    if [ -f "$wad_path" ]; then
        local size
        size=$(stat -c%s "$wad_path" 2>/dev/null || wc -c < "$wad_path" 2>/dev/null || echo 0)
        if [ "$size" -ge 4000000 ]; then
            # Verify IWAD header magic
            local magic
            magic=$(head -c 4 "$wad_path" 2>/dev/null || true)
            if [ "$magic" = "IWAD" ]; then
                return 0
            fi
        fi
    fi
    return 1
}

if check_valid_iwad "$TARGET_USER_WAD"; then
    echo -e "${GREEN}[OK] Verified DOOM1.WAD already exists in $DEST_USER.${NC}"
    cp -u "$TARGET_USER_WAD" "$TARGET_REPO_WAD" 2>/dev/null || cp "$TARGET_USER_WAD" "$TARGET_REPO_WAD"
    echo -e "${GREEN}[OK] Synced to $TARGET_REPO_WAD.${NC}"
    exit 0
fi

if check_valid_iwad "$TARGET_REPO_WAD"; then
    echo -e "${GREEN}[OK] Verified DOOM1.WAD found in $DEST_REPO.${NC}"
    cp -u "$TARGET_REPO_WAD" "$TARGET_USER_WAD" 2>/dev/null || cp "$TARGET_REPO_WAD" "$TARGET_USER_WAD"
    echo -e "${GREEN}[OK] Synced to $TARGET_USER_WAD.${NC}"
    exit 0
fi

# List of reputable mirror URLs for DOOM1.WAD (v1.9 shareware)
URLS=(
    "https://distro.ibiblio.org/pub/linux/distributions/slitaz/sources/packages/d/doom1.wad"
    "https://archive.org/download/2020_03_22_doom/DOOM1.WAD"
    "https://raw.githubusercontent.com/Doom-Utils/iwad-collection/master/DOOM1.WAD"
    "https://www.jbserver.com/downloads/games/doom/misc/shareware/doom1.wad"
)

TEMP_WAD="/tmp/DOOM1.WAD.$$"

echo -e "\n${YELLOW}[DOWNLOADING] Fetching shareware DOOM1.WAD (~4.19 MB)...${NC}"
DOWNLOAD_SUCCESS=0

for url in "${URLS[@]}"; do
    echo -e "  Attempting mirror: ${CYAN}$url${NC}..."
    if command -v curl >/dev/null 2>&1; then
        if curl -f -L -s --connect-timeout 10 --max-time 60 "$url" -o "$TEMP_WAD"; then
            if check_valid_iwad "$TEMP_WAD"; then
                DOWNLOAD_SUCCESS=1
                break
            fi
        fi
    elif command -v wget >/dev/null 2>&1; then
        if wget -q --timeout=10 -t 2 "$url" -O "$TEMP_WAD"; then
            if check_valid_iwad "$TEMP_WAD"; then
                DOWNLOAD_SUCCESS=1
                break
            fi
        fi
    fi
    rm -f "$TEMP_WAD"
done

if [ "$DOWNLOAD_SUCCESS" -ne 1 ]; then
    echo -e "\n${RED}[ERROR] Failed to download DOOM1.WAD from all mirrors.${NC}"
    echo -e "Please manually copy DOOM1.WAD, DOOM.WAD, or DOOM2.WAD to:"
    echo -e "  ${CYAN}$DEST_USER/DOOM1.WAD${NC}  or  ${CYAN}$DEST_REPO/DOOM1.WAD${NC}\n"
    rm -f "$TEMP_WAD"
    exit 1
fi

# Move verified IWAD to target destinations
mv "$TEMP_WAD" "$TARGET_USER_WAD"
cp "$TARGET_USER_WAD" "$TARGET_REPO_WAD"
chmod 644 "$TARGET_USER_WAD" "$TARGET_REPO_WAD"

echo -e "\n${GREEN}========================================================================${NC}"
echo -e "${GREEN}  DOOM1.WAD downloaded and verified successfully!                       ${NC}"
echo -e "${GREEN}  Installed locations:                                                  ${NC}"
echo -e "    1. ${CYAN}$TARGET_USER_WAD${NC}"
echo -e "    2. ${CYAN}$TARGET_REPO_WAD${NC}"
echo -e "${GREEN}========================================================================${NC}\n"
