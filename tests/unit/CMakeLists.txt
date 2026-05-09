# CMakeLists.txt for unit tests

cmake_minimum_required(VERSION 3.22)

# ROS2 dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)

# Add ROS2 include/lib directories
get_filename_component(ROS_BASE_DIR "${rclcpp_DIR}/../../../" ABSOLUTE)
include_directories(SYSTEM "${ROS_BASE_DIR}/include")
link_directories("${ROS_BASE_DIR}/lib")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Common includes for all unit tests
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/data_collection
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/rl_planning_infer
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/rl_planning_infer/observation
    ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty
    ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/nlohmann
    ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/nlohmann/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/ruckig/include
)

link_directories(${CMAKE_BINARY_DIR}/src)

# ========== Build GTest from ROS2 vendor sources ==========
set(GTEST_SOURCE_DIR "${ROS_BASE_DIR}/src/gtest_vendor")

add_library(gtest STATIC
    ${GTEST_SOURCE_DIR}/src/gtest.cc
    ${GTEST_SOURCE_DIR}/src/gtest-death-test.cc
    ${GTEST_SOURCE_DIR}/src/gtest-matchers.cc
    ${GTEST_SOURCE_DIR}/src/gtest-filepath.cc
    ${GTEST_SOURCE_DIR}/src/gtest-port.cc
    ${GTEST_SOURCE_DIR}/src/gtest-printers.cc
    ${GTEST_SOURCE_DIR}/src/gtest-test-part.cc
    ${GTEST_SOURCE_DIR}/src/gtest-typed-test.cc
)
target_include_directories(gtest SYSTEM PUBLIC
    ${GTEST_SOURCE_DIR}/include
    ${GTEST_SOURCE_DIR}
)
target_compile_options(gtest PRIVATE -Wno-missing-field-initializers)

add_library(gtest_main STATIC
    ${GTEST_SOURCE_DIR}/src/gtest_main.cc
)
target_link_libraries(gtest_main PUBLIC gtest)

# Helper function to create a unit test
function(add_unit_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    ament_target_dependencies(${TEST_NAME} rclcpp)
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_BINARY_DIR}/src/rosidl_generator_cpp
        ${CMAKE_BINARY_DIR}/src/rosidl_generator_c
    )
    target_link_libraries(${TEST_NAME}
        aurora_edge_runtime
        gtest_main
        ruckig::ruckig
        Boost::filesystem
        Boost::system
        Boost::thread
        pthread
    )
    # Link against generated interface libraries if available
    if(TARGET aurora_edge_interfaces__rosidl_typesupport_cpp)
        target_link_libraries(${TEST_NAME}
            aurora_edge_interfaces__rosidl_typesupport_cpp
            rcl
        )
    endif()
    if(TARGET aurora_edge_interfaces__rosidl_typesupport_fastrtps_cpp)
        target_link_libraries(${TEST_NAME}
            aurora_edge_interfaces__rosidl_typesupport_fastrtps_cpp
        )
    endif()
    set_target_properties(${TEST_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# ========== Group 1: Pure Logic Modules ==========

add_unit_test(test_state_machine test_state_machine.cpp)
add_unit_test(test_error_handler test_error_handler.cpp)
add_unit_test(test_ring_buffer test_ring_buffer.cpp)

# ========== Group 2: Gait Modules ==========

add_unit_test(test_safety_system test_safety_system.cpp)
add_unit_test(test_foot_trajectory test_foot_trajectory.cpp)
add_unit_test(test_kinematics test_kinematics.cpp)

# ========== Group 3: Data Collection ==========

add_unit_test(test_data_collection_executor test_data_collection_executor.cpp)
add_unit_test(test_ros2bag_recorder test_ros2bag_recorder.cpp)
add_unit_test(test_file_compress test_file_compress.cpp)
add_unit_test(test_data_encryption test_data_encryption.cpp)

# add_unit_test(test_data_manager test_data_manager.cpp)
# add_unit_test(test_collection_feedback test_collection_feedback.cpp)
