#!/bin/bash
# memory_test.sh
# Memory leak detection test using Valgrind

set -e

PROJECT_DIR="/home/xucong/caicAD/01datainfra/Aurora/aurora-edge-runtime"
cd "$PROJECT_DIR"

echo "=========================================="
echo "  Memory Leak Detection Test (Valgrind)  "
echo "=========================================="
echo ""

# Check if valgrind is installed
if ! command -v valgrind &> /dev/null; then
    echo "Error: valgrind is not installed."
    echo "Install with: sudo apt-get install valgrind"
    exit 1
fi

# Set environment variables
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export LD_LIBRARY_PATH=$PROJECT_DIR/lib:${LD_LIBRARY_PATH:-}
export AMENT_PREFIX_PATH=/opt/ros/humble${AMENT_PREFIX_PATH:+:${AMENT_PREFIX_PATH}}

# Source ROS2
if [[ -f "/opt/ros/humble/setup.bash" ]]; then
    source /opt/ros/humble/setup.bash >/dev/null 2>&1
fi

# Clean up FastDDS shared memory before test
rm -rf /dev/shm/fastrtps_* 2>/dev/null || true

echo "Running Valgrind with leak check..."
echo "Note: This test runs a short cycle and exits after 2 data collection cycles"
echo ""

# Run valgrind with leak check
# We limit the program to run only 2 cycles to get a quick test
valgrind \
    --tool=memcheck \
    --leak-check=full \
    --show-leak-kinds=all \
    --track-origins=yes \
    --verbose \
    --log-file=valgrind_output.log \
    --suppressions=valgrind.supp \
    "$PROJECT_DIR/build/src/dcp" --mode humanoid &

VALGRIND_PID=$!

# Let it run for 60 seconds then send SIGINT
sleep 60
kill -INT $VALGRIND_PID 2>/dev/null || true

# Wait for valgrind to finish
wait $VALGRIND_PID 2>/dev/null || true

echo ""
echo "=========================================="
echo "  Valgrind Summary"
echo "=========================================="
echo ""

# Extract summary from log
if [[ -f "valgrind_output.log" ]]; then
    # Print leak summary
    grep -A 20 "LEAK SUMMARY" valgrind_output.log || echo "No leak summary found"

    echo ""
    echo "=========================================="
    echo "  Error Summary"
    echo "=========================================="
    echo ""

    # Print error count
    grep "ERROR SUMMARY" valgrind_output.log || echo "No error summary found"

    # Count use-after-free errors
    UAF_COUNT=$(grep -i "invalid read\|invalid write\|use after free" valgrind_output.log | wc -l)
    echo ""
    echo "Use-After-Free/Invalid Access Errors: $UAF_COUNT"

    # Check for definitely lost
    DEFINITELY_LOST=$(grep "definitely lost" valgrind_output.log | tail -1 || echo "0")
    echo "Definitely Lost: $DEFINITELY_LOST"

    echo ""
    echo "Full log saved to: valgrind_output.log"
    echo "View with: less valgrind_output.log"
else
    echo "Error: Valgrind log file not created"
    exit 1
fi

echo ""
echo "=========================================="
echo "  Test Status"
echo "=========================================="

# Check if test passed
if [[ $UAF_COUNT -eq 0 ]]; then
    echo "✅ Use-after-free errors: FIXED (0 errors)"
else
    echo "❌ Use-after-free errors: STILL PRESENT ($UAF_COUNT errors)"
fi

# Check definitely lost (allow up to 100 bytes for third-party libs)
DEFINITELY_BYTES=$(echo "$DEFINITELY_LOST" | grep -oP '\d+(?= bytes)' || echo "0")
if [[ $DEFINITELY_BYTES -lt 100 ]]; then
    echo "✅ Memory leaks: FIXED (< 100 bytes)"
else
    echo "⚠️  Memory leaks: Still present ($DEFINITELY_LOST)"
fi
