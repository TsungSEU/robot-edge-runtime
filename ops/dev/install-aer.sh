#!/bin/bash
#
# install-aer.sh - Install aer command to system PATH
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
AER_SCRIPT="$SCRIPT_DIR/aer"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

# Check if aer script exists
if [ ! -f "$AER_SCRIPT" ]; then
    echo -e "${RED}Error: aer script not found at $AER_SCRIPT${NC}"
    exit 1
fi

# Try different installation locations
INSTALL_DIR=""
USE_SUDO=""

if [ -w /usr/local/bin ] || [ "$(id -u)" = "0" ]; then
    INSTALL_DIR="/usr/local/bin"
    USE_SUDO=""
    echo -e "${GREEN}Installing to system directory: $INSTALL_DIR${NC}"
else
    # Try with sudo
    INSTALL_DIR="/usr/local/bin"
    USE_SUDO="sudo"
    echo -e "${YELLOW}Installing to system directory (requires sudo): $INSTALL_DIR${NC}"
fi

# Create symlink
$USE_SUDO ln -sf "$AER_SCRIPT" "$INSTALL_DIR/aer"

# Make sure it's executable
$USE_SUDO chmod +x "$INSTALL_DIR/aer"

echo -e "${GREEN}✓ aer command installed successfully!${NC}"
echo ""

# Check if INSTALL_DIR is in PATH
if [[ ":$PATH:" != *":$INSTALL_DIR:"* ]]; then
    echo -e "${YELLOW}⚠ Warning: $INSTALL_DIR is not in your PATH${NC}"
    echo ""
    echo "Add the following to your ~/.bashrc or ~/.zshrc:"
    echo "  export PATH=\"$INSTALL_DIR:\$PATH\""
    echo ""
    echo "Then run: source ~/.bashrc  (or source ~/.zshrc)"
    echo ""
fi

echo "Usage:"
echo "  aer start              # Start all services"
echo "  aer stop               # Stop all services"
echo "  aer status             # Show service status"
echo "  aer logs <service>     # Follow logs"
echo "  aer --help             # Show all options"
echo ""
echo -e "${YELLOW}Note: You may need to open a new terminal for changes to take effect.${NC}"
