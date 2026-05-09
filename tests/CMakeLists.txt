# CMakeLists.txt for unit tests

cmake_minimum_required(VERSION 3.22)

# 查找ROS2依赖
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)

# 添加ROS2的include和lib目录
get_filename_component(ROS_BASE_DIR "${rclcpp_DIR}/../../../" ABSOLUTE)
include_directories(SYSTEM "${ROS_BASE_DIR}/include")
link_directories("${ROS_BASE_DIR}/lib")

# 设置 C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/data_collection
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/rl_planning_infer
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/rl_planning_infer/observation
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/nlohmann
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/nlohmann/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/ruckig/include
)

# 链接目录
link_directories(
    ${CMAKE_BINARY_DIR}/src
)

# ========== DCP Integration Test ==========
# DCP Full System Integration Test
message(STATUS "Adding DCP Integration Test...")

set(DCP_INTEGRATION_TEST_SOURCES
    test_dcp_integration.cpp
)

# DCP Integration Test executable
add_executable(test_dcp_integration ${DCP_INTEGRATION_TEST_SOURCES})

# 使用ament_target_dependencies处理ROS2依赖
ament_target_dependencies(test_dcp_integration
    rclcpp
)

# Add include directories for generated service headers to test executable
target_include_directories(test_dcp_integration PRIVATE
    ${CMAKE_BINARY_DIR}/src/rosidl_generator_cpp
    ${CMAKE_BINARY_DIR}/src/rosidl_generator_c
)

# Link libraries for DCP Integration Test (combine all in one call)
target_link_libraries(test_dcp_integration
    aurora_edge_runtime
    ruckig::ruckig
    Boost::filesystem
    Boost::system
    Boost::thread
    pthread
)

# Link against the generated interface libraries and ROS2 client library
# Need both cpp and fastrtps typesupport for services
if(TARGET aurora_edge_interfaces__rosidl_typesupport_cpp)
    target_link_libraries(test_dcp_integration
        aurora_edge_interfaces__rosidl_typesupport_cpp
        rcl
    )
endif()
if(TARGET aurora_edge_interfaces__rosidl_typesupport_fastrtps_cpp)
    target_link_libraries(test_dcp_integration
        aurora_edge_interfaces__rosidl_typesupport_fastrtps_cpp
    )
endif()

# Set output directory
set_target_properties(test_dcp_integration PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

# Add CTest
add_test(NAME DCPIntegrationTest COMMAND test_dcp_integration)

message(STATUS "DCP Integration Test: test_dcp_integration")

# Unit tests
add_subdirectory(unit)

# Integration tests
add_subdirectory(integration)
