# CMakeLists.txt for integration tests

cmake_minimum_required(VERSION 3.22)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)

get_filename_component(ROS_BASE_DIR "${rclcpp_DIR}/../../../" ABSOLUTE)
include_directories(SYSTEM "${ROS_BASE_DIR}/include")
link_directories("${ROS_BASE_DIR}/lib")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# Reuse gtest from unit tests
set(GTEST_SOURCE_DIR "${ROS_BASE_DIR}/src/gtest_vendor")

# Helper function for integration tests (reuse gtest from parent)
function(add_integration_test TEST_NAME TEST_SOURCE)
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

add_integration_test(test_gait_trigger_chain test_gait_trigger_chain.cpp)
add_integration_test(test_data_upload_chain test_data_upload_chain.cpp)
