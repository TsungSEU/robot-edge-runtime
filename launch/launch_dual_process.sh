#!/bin/bash
# Shell script to launch both robot_sim and dcp processes
# This script provides a simple alternative to the Python launch file

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}================================================${NC}"
echo -e "${GREEN}  Aurora Edge Runtime - Dual Process Launch${NC}"
echo -e "${GREEN}================================================${NC}"
echo ""

# Source ROS2 environment
if [ -f "/opt/ros/humble/setup.bash" ]; then
    echo -e "${GREEN}Sourcing ROS2 Humble environment...${NC}"
    source /opt/ros/humble/setup.bash
else
    echo -e "${YELLOW}Warning: ROS2 Humble setup.bash not found at /opt/ros/humble/setup.bash${NC}"
    echo -e "${YELLOW}Attempting to continue without ROS2 environment...${NC}"
fi

# Source project environment
if [ -f "$PROJECT_ROOT/resource/scripts/setup.bash" ]; then
    echo -e "${GREEN}Sourcing project environment...${NC}"
    source "$PROJECT_ROOT/resource/scripts/setup.bash"
fi

echo ""

# Set library path to find generated interface libraries
export LD_LIBRARY_PATH="$BUILD_DIR/src:$LD_LIBRARY_PATH"

# Check if executables exist
if [ ! -f "$BUILD_DIR/src/robot_sim" ]; then
    echo -e "${RED}Error: robot_sim executable not found at $BUILD_DIR/src/robot_sim${NC}"
    echo -e "${YELLOW}Please build the project first:${NC}"
    echo "  cd $BUILD_DIR && cmake .. && make -j8"
    exit 1
fi

if [ ! -f "$BUILD_DIR/src/dcp" ]; then
    echo -e "${RED}Error: dcp executable not found at $BUILD_DIR/src/dcp${NC}"
    echo -e "${YELLOW}Please build the project first:${NC}"
    echo "  cd $BUILD_DIR && cmake .. && make -j8"
    exit 1
fi

# Function to cleanup on exit
cleanup() {
    echo ""
    echo -e "${YELLOW}Stopping processes...${NC}"
    # Kill background processes
    jobs -p | xargs -r kill 2>/dev/null
    wait 2>/dev/null
    echo -e "${GREEN}Done.${NC}"
}

# Set trap to cleanup on script exit
trap cleanup EXIT INT TERM

# Launch robot_sim in background
echo -e "${GREEN}[1/2]${NC} Starting robot_sim..."
"$BUILD_DIR/src/robot_sim" &
ROBOT_SIM_PID=$!

# Wait for robot_sim to initialize (2 seconds)
echo -e "${YELLOW}Waiting for robot_sim to initialize...${NC}"
sleep 2

# Check if robot_sim is still running
if ! kill -0 $ROBOT_SIM_PID 2>/dev/null; then
    echo -e "${RED}Error: robot_sim failed to start${NC}"
    exit 1
fi

echo -e "${GREEN}[2/2]${NC} Starting dcp..."
"$BUILD_DIR/src/dcp" &
DCP_PID=$!

echo ""
echo -e "${GREEN}================================================${NC}"
echo -e "${GREEN}  Both processes started successfully!${NC}"
echo -e "${GREEN}================================================${NC}"
echo ""
echo "robot_sim PID: $ROBOT_SIM_PID"
echo "dcp PID: $DCP_PID"
echo ""
echo -e "${YELLOW}Press Ctrl+C to stop both processes${NC}"
echo ""

# Wait for any process to exit
wait -n
EXIT_CODE=$?

echo ""
if [ $EXIT_CODE -ne 0 ]; then
    echo -e "${RED}A process exited with code $EXIT_CODE${NC}"
fi

exit $EXIT_CODE
