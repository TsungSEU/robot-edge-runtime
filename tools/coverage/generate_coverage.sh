#!/bin/bash
# Coverage Report Generator for Aurora Edge Runtime
# Generates code coverage reports using gcov and lcov

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Aurora Edge Runtime - Coverage Generator${NC}"
echo -e "${BLUE}========================================${NC}"

# Project root
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$PROJECT_ROOT"

# Build directory
BUILD_DIR="${PROJECT_ROOT}/build_coverage"
RESULTS_DIR="${PROJECT_ROOT}/tools/coverage/results"

# Clean previous coverage data
echo -e "\n${YELLOW}[1/6] Cleaning previous coverage data...${NC}"
rm -rf "$BUILD_DIR"
rm -rf "$RESULTS_DIR"
mkdir -p "$RESULTS_DIR"

# Configure with coverage enabled
echo -e "\n${YELLOW}[2/6] Configuring build with coverage flags...${NC}"
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_COVERAGE=ON \
    -DBUILD_TESTS=ON \
    -DENABLE_ROS2=ON

# Build with coverage
echo -e "\n${YELLOW}[3/6] Building with coverage instrumentation...${NC}"
cmake --build "$BUILD_DIR" -j$(nproc)

# Reset coverage data
echo -e "\n${YELLOW}[4/6] Resetting coverage counters...${NC}"
lcov --directory "$BUILD_DIR" --zerocounters

# Run tests
echo -e "\n${YELLOW}[5/6] Running tests...${NC}"
export GTEST_OUTPUT="test:${RESULTS_DIR}/gtest_results.xml"
cd "$BUILD_DIR"
ctest --output-on-failure --verbose 2>&1 | tee "$RESULTS_DIR/ctest.log"

# Generate coverage report
echo -e "\n${YELLOW}[6/6] Generating coverage report...${NC}"
cd "$PROJECT_ROOT"

# Capture coverage data
lcov --directory "$BUILD_DIR" --capture --output-file "$RESULTS_DIR/coverage.info" \
    --rc lcov_branch_coverage=1

# Remove system and test files from coverage
lcov --remove "$RESULTS_DIR/coverage.info" \
    '/usr/*' \
    '/opt/*' \
    '*/tests/*' \
    '*/3rdparty/*' \
    '*/build_coverage/*' \
    '*/install/*' \
    '*/log/*' \
    --output-file "$RESULTS_DIR/coverage_filtered.info" \
    --rc lcov_branch_coverage=1

# Generate HTML report
genhtml "$RESULTS_DIR/coverage_filtered.info" \
    --output-directory "$RESULTS_DIR/html" \
    --rc lcov_branch_coverage=1 \
    --title "Aurora Edge Runtime - Code Coverage Report" \
    --legend \
    --show-details

# Calculate coverage percentage
LINE_COV=$(lcov --summary "$RESULTS_DIR/coverage_filtered.info" 2>&1 | grep "lines" | awk '{print $2}' | sed 's/%//')
BRANCH_COV=$(lcov --summary "$RESULTS_DIR/coverage_filtered.info" 2>&1 | grep "functions" | awk '{print $2}' | sed 's/%//')

# Print summary
echo -e "\n${GREEN}========================================${NC}"
echo -e "${GREEN}Coverage Summary${NC}"
echo -e "${GREEN}========================================${NC}"
lcov --summary "$RESULTS_DIR/coverage_filtered.info"

echo -e "\n${GREEN}Coverage Report Generated:${NC}"
echo -e "  HTML Report: ${BLUE}file://$RESULTS_DIR/html/index.html${NC}"
echo -e "  Coverage Data: ${BLUE}$RESULTS_DIR/coverage_filtered.info${NC}"

# Check if coverage meets the 40% target
echo -e "\n${BLUE}Coverage Target Check:${NC}"
TARGET_COVERAGE=40.0
if (( $(echo "$LINE_COV >= $TARGET_COVERAGE" | bc -l) )); then
    echo -e "  ${GREEN}✓ Line coverage: ${LINE_COV}% (Target: ${TARGET_COVERAGE}%)${NC}"
    exit 0
else
    echo -e "  ${RED}✗ Line coverage: ${LINE_COV}% (Target: ${TARGET_COVERAGE}%)${NC}"
    echo -e "  ${YELLOW}  Gap: $(echo "$TARGET_COVERAGE - $LINE_COV" | bc)% needed${NC}"
    exit 1
fi
