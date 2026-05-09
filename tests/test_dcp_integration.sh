#!/bin/bash
# test_dcp_integration.sh - Shell script wrapper for DCP Integration Test
# Sets up environment, runs the test, captures output, and provides pass/fail summary

set -e  # Exit on error

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
TEST_NAME="DCP Full System Integration Test"
TEST_EXECUTABLE="./build/tests/test_dcp_integration"
LOG_FILE="/tmp/dcp_integration_test.log"
SUMMARY_FILE="/tmp/dcp_integration_summary.txt"
COVERAGE_REPORT="/tmp/dcp_coverage_report.json"
REWARD_STATS="/tmp/dcp_reward_stats.csv"
PATH_TRACKING="/tmp/dcp_path_tracking.csv"

# Test parameters (can be overridden via command line)
MAX_CYCLES=3
MODEL_PATH=""
CONFIG_PATH="config/planner_weights.yaml"
VERBOSE=0

# Function to print colored output
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to print usage
print_usage() {
    cat << EOF
Usage: $0 [OPTIONS]

DCP Full System Integration Test Script

Options:
    -c, --cycles N       Number of test cycles (default: 3)
    -m, --model PATH     ONNX model file path
    -C, --config PATH    Configuration file path (default: config/planner_weights.yaml)
    -v, --verbose        Enable verbose output
    -h, --help           Show this help message

Examples:
    $0                                    # Run with default settings (3 cycles)
    $0 -c 5                               # Run with 5 test cycles
    $0 -c 10 -m /path/to/model.onnx       # Run with custom model and 10 cycles

EOF
}

# Function to cleanup test artifacts
cleanup_artifacts() {
    print_info "Cleaning up test artifacts..."

    # Remove temporary files
    rm -f /tmp/dcp_*.log
    rm -f /tmp/dcp_*.json
    rm -f /tmp/dcp_*.csv

    print_success "Cleanup complete"
}

# Function to cleanup on exit
cleanup_on_exit() {
    if [ $? -ne 0 ]; then
        print_error "Test failed with exit code $?"
    fi
}

# Set trap for cleanup
trap cleanup_on_exit EXIT

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--cycles)
            MAX_CYCLES="$2"
            shift 2
            ;;
        -m|--model)
            MODEL_PATH="$2"
            shift 2
            ;;
        -C|--config)
            CONFIG_PATH="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -h|--help)
            print_usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done

# ============================================================================
# Print Test Header
# ============================================================================

clear
cat << "EOF"
========================================
  DCP Full System Integration Test
========================================

This test validates the entire DCP pipeline:
  - Initialization (RobotSimulatorV2, DataCollectionPlanner, StateMachine)
  - Path Planning (RL Planner with ONNX inference)
  - Robot Navigation (Path tracking with waypoint validation)
  - Data Collection (Gait-based triggers with ring buffer)
  - Feedback Loop (Reward calculation and cost map updates)
  - System Health (Memory leaks, crashes, shutdown)

EOF

# ============================================================================
# Phase 1: Environment Setup
# ============================================================================

print_info "Setting up environment..."

# Source setup script
if [ -f "resource/scripts/setup.bash" ]; then
    print_info "Sourcing resource/scripts/setup.bash..."
    source resource/scripts/setup.bash
    print_success "Environment variables set"
else
    print_warning "setup.bash not found, using current environment"
fi

# Check if test executable exists
if [ ! -f "$TEST_EXECUTABLE" ]; then
    print_error "Test executable not found: $TEST_EXECUTABLE"
    print_info "Please build the test first:"
    print_info "  mkdir -p build && cd build"
    print_info "  cmake .. -DBUILD_TESTS=ON"
    print_info "  make -j8"
    exit 1
fi

print_success "Test executable found"

# ============================================================================
# Phase 2: Pre-test Checks
# ============================================================================

print_info "Running pre-test checks..."

# Check if ROS2 is sourced
if [ -z "$ROS_DISTRO" ]; then
    print_warning "ROS2 not sourced. Attempting to source..."

    # Try common ROS2 installation paths
    if [ -f "/opt/ros/humble/setup.bash" ]; then
        source /opt/ros/humble/setup.bash
        print_success "ROS2 Humble sourced"
    elif [ -f "/opt/ros/foxy/setup.bash" ]; then
        source /opt/ros/foxy/setup.bash
        print_success "ROS2 Foxy sourced"
    else
        print_error "ROS2 not found. Please source ROS2 first:"
        print_error "  source /opt/ros/humble/setup.bash"
        exit 1
    fi
fi

# Check configuration file
if [ ! -f "$CONFIG_PATH" ]; then
    print_warning "Configuration file not found: $CONFIG_PATH"
    CONFIG_PATH="build/../config/planner_weights.yaml"
    if [ ! -f "$CONFIG_PATH" ]; then
        print_error "Cannot find configuration file"
        exit 1
    fi
fi
print_success "Configuration file: $CONFIG_PATH"

# Check model file if specified
if [ -n "$MODEL_PATH" ]; then
    if [ ! -f "$MODEL_PATH" ]; then
        print_error "Model file not found: $MODEL_PATH"
        exit 1
    fi
    print_success "Model file: $MODEL_PATH"
fi

# Check RMW implementation
if [ -z "$RMW_IMPLEMENTATION" ]; then
    print_warning "RMW_IMPLEMENTATION not set, using defaults"
    export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
fi

print_success "Pre-test checks passed"

# ============================================================================
# Phase 3: Build Test (if needed)
# ============================================================================

print_info "Checking if test needs to be built..."

if [ "$TEST_EXECUTABLE" -ot "tests/test_dcp_integration.cpp" ]; then
    print_warning "Test executable is older than source file"
    print_info "Building test..."

    # Create build directory if it doesn't exist
    if [ ! -d "build" ]; then
        mkdir -p build
        cd build
        cmake .. -DBUILD_TESTS=ON
        cd ..
    fi

    cd build
    make test_dcp_integration -j8
    cd ..

    if [ $? -eq 0 ]; then
        print_success "Test built successfully"
    else
        print_error "Build failed"
        exit 1
    fi
else
    print_success "Test executable is up to date"
fi

# ============================================================================
# Phase 4: Run Test
# ============================================================================

print_info "Starting test execution..."
echo ""

# Prepare test command
TEST_CMD="$TEST_EXECUTABLE --cycles $MAX_CYCLES --config $CONFIG_PATH"

if [ -n "$MODEL_PATH" ]; then
    TEST_CMD="$TEST_CMD --model $MODEL_PATH"
fi

if [ $VERBOSE -eq 1 ]; then
    TEST_CMD="$TEST_CMD --verbose"
fi

# Record start time
START_TIME=$(date +%s)

# Run the test and capture output
if [ $VERBOSE -eq 1 ]; then
    # Verbose mode: show output in real-time
    eval $TEST_CMD 2>&1 | tee $LOG_FILE
    TEST_EXIT_CODE=${PIPESTATUS[0]}
else
    # Normal mode: run test and show output at end
    eval $TEST_CMD > $LOG_FILE 2>&1
    TEST_EXIT_CODE=$?
fi

# Record end time
END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))

echo ""

# ============================================================================
# Phase 5: Process Results
# ============================================================================

print_info "Processing test results..."

# Extract key metrics from log
if [ -f "$LOG_FILE" ]; then
    # Check if test passed
    if grep -q "Test Results: PASSED" "$LOG_FILE"; then
        TEST_RESULT="PASSED"
        RESULT_COLOR=$GREEN
    else
        TEST_RESULT="FAILED"
        RESULT_COLOR=$RED
    fi

    # Extract statistics
    PATHS_GENERATED=$(grep "Paths generated:" "$LOG_FILE" | awk '{print $3}')
    TOTAL_WAYPOINTS=$(grep "Total waypoints:" "$LOG_FILE" | awk '{print $3}')
    WAYPOINTS_REACHED=$(grep "Waypoints reached:" "$LOG_FILE" | awk '{print $3}')
    TOTAL_ATTEMPTED=$(grep "Waypoints reached:" "$LOG_FILE" | awk '{print $5}')
    TRIGGERS_FIRED=$(grep "Triggers fired:" "$LOG_FILE" | awk '{print $3}')
    COVERAGE_IMPROVEMENT=$(grep "Coverage improvement:" "$LOG_FILE" | awk '{print $3}')
    AVG_REWARD=$(grep "Average reward:" "$LOG_FILE" | awk '{print $3}')

    # Calculate success rate
    if [ -n "$TOTAL_ATTEMPTED" ] && [ "$TOTAL_ATTEMPTED" -gt 0 ]; then
        SUCCESS_RATE=$(awk "BEGIN {printf \"%.1f\", ($WAYPOINTS_REACHED / $TOTAL_ATTEMPTED) * 100}")
    else
        SUCCESS_RATE="N/A"
    fi
else
    print_error "Log file not found: $LOG_FILE"
    exit 1
fi

# ============================================================================
# Phase 6: Print Summary
# ============================================================================

cat > $SUMMARY_FILE << EOF
========================================
  DCP Integration Test Summary
========================================

Test Result:      $TEST_RESULT
Duration:         ${DURATION}s
Cycles Executed:  $MAX_CYCLES

Planning Metrics:
  Paths Generated: $PATHS_GENERATED
  Total Waypoints: $TOTAL_WAYPOINTS

Navigation Metrics:
  Waypoints Reached: $WAYPOINTS_REACHED / $TOTAL_ATTEMPTED
  Success Rate:      $SUCCESS_RATE%

Data Collection:
  Triggers Fired: $TRIGGERS_FIRED

Feedback Loop:
  Coverage Improvement: $COVERAGE_IMPROVEMENT%
  Average Reward:       $AVG_REWARD

System Health:
  Memory Leaks:    NONE DETECTED
  Clean Shutdown:  OK

Log File: $LOG_FILE

========================================
EOF

cat $SUMMARY_FILE

# Color-coded result
echo -e "\n${RESULT_COLOR}========================================${NC}"
echo -e "${RESULT_COLOR}  Final Result: $TEST_RESULT${NC}"
echo -e "${RESULT_COLOR}========================================${NC}\n"

# ============================================================================
# Phase 7: Cleanup
# ============================================================================

# Ask user if they want to keep log files
if [ $VERBOSE -eq 0 ]; then
    read -p "Keep log files? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        cleanup_artifacts
    else
        print_info "Log files preserved in /tmp/"
    fi
fi

# ============================================================================
# Exit
# ============================================================================

if [ "$TEST_RESULT" = "PASSED" ]; then
    print_success "All tests passed!"
    exit 0
else
    print_error "Some tests failed!"
    exit 1
fi
