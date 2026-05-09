# Code Coverage Guide - Aurora Edge Runtime

This directory contains tools and documentation for generating code coverage reports using gcov and lcov.

## Quick Start

### Generate Coverage Report

```bash
./tools/coverage/generate_coverage.sh
```

This will:
1. Clean previous coverage data
2. Configure build with coverage flags (`--coverage -O0 -g`)
3. Build all tests with instrumentation
4. Run all tests via CTest
5. Generate HTML coverage report in `tools/coverage/results/html/`

### View Coverage Report

```bash
# Open in browser
xdg-open tools/coverage/results/html/index.html

# Or serve with HTTP server
cd tools/coverage/results/html && python3 -m http.server 8000
# Then open http://localhost:8000
```

## Coverage Target

**Current Goal:** 40% line coverage across all source files

### Tracking Progress

| Metric | Current | Target | Status |
|--------|---------|--------|--------|
| Line Coverage | TBD | 40% | 🎯 In Progress |
| Branch Coverage | TBD | 30% | 🎯 In Progress |
| Function Coverage | TBD | 50% | 🎯 In Progress |

## Manual Coverage Generation

### Step-by-Step

```bash
# 1. Configure with coverage
mkdir -p build_coverage && cd build_coverage
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON -DBUILD_TESTS=ON

# 2. Build
make -j$(nproc)

# 3. Reset counters
lcov --directory . --zerocounters

# 4. Run tests
ctest --output-on-failure

# 5. Capture coverage
lcov --directory . --capture --output-file coverage.info

# 6. Filter out system files
lcov --remove coverage.info \
    '/usr/*' \
    '*/tests/*' \
    '*/3rdparty/*' \
    --output-file coverage_filtered.info

# 7. Generate HTML
genhtml coverage_filtered.info --output-directory coverage_html
```

## Test Files

### Unit Tests (`tests/unit/`)

- `test_state_machine.cpp` - State machine transitions
- `test_error_handler.cpp` - Error handling and logging
- `test_ring_buffer.cpp` - Lock-free ring buffer
- `test_safety_system.cpp` - Safety system limits
- `test_foot_trajectory.cpp` - Foot trajectory generation
- `test_kinematics.cpp` - Inverse kinematics

### Integration Tests (`tests/integration/`)

- `test_gait_trigger_chain.cpp` - Gait trigger integration
- `test_dcp_integration.cpp` - Full DCP system integration

## Coverage Exclusions

The following are excluded from coverage calculations:

- System headers (`/usr/*`)
- Test files themselves (`*/tests/*`)
- Third-party dependencies (`*/3rdparty/*`)
- Build artifacts (`*/build_coverage/*`)
- ROS2 generated files

## Interpreting Results

### Coverage Types

- **Line Coverage**: Percentage of executable lines executed
- **Branch Coverage**: Percentage of branches (if/else) taken
- **Function Coverage**: Percentage of functions called

### HTML Report Legend

- <span style="color:green">Green</span>: High coverage (>80%)
- <span style="color:yellow">Yellow</span>: Medium coverage (50-80%)
- <span style="color:red">Red</span>: Low coverage (<50%)

## CI/CD Integration

To integrate coverage into CI pipeline:

```yaml
# Example GitLab CI snippet
test:coverage:
  script:
    - ./tools/coverage/generate_coverage.sh
  coverage: '/lines\.*: (\d+\.\d+)%/'
  artifacts:
    paths:
      - tools/coverage/results/html/
    reports:
      coverage_report:
        coverage_format: cobertura
        path: tools/coverage/results/coverage.xml
```

## Troubleshooting

### No coverage data generated

- Ensure `ENABLE_COVERAGE=ON` is set
- Check that tests actually run (use `ctest --verbose`)
- Verify gcov is installed: `which gcov`

### Missing coverage for specific files

- Check file is compiled with `-fprofile-arcs -ftest-coverage`
- Verify `.gcda` files are generated after test run
- Check file paths in lcov exclude patterns

### Build fails with coverage enabled

- Ensure sufficient disk space (coverage data is large)
- Check for non-POSIX compliant code in coverage paths
- Try disabling optimization: `-DCMAKE_CXX_FLAGS="-O0 -g"`

## References

- [lcov Manual](http://ltp.sourceforge.net/coverage/lcov.php)
- [gcov Documentation](https://gcc.gnu.org/onlinedocs/gcc/Gcov.html)
- [CTest Documentation](https://cmake.org/cmake/help/latest/manual/ctest.1.html)
