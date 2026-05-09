include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/data_collection
    ${CMAKE_CURRENT_SOURCE_DIR}/rl_planning_infer
    ${CMAKE_CURRENT_SOURCE_DIR}/state_machine
    ${CMAKE_CURRENT_SOURCE_DIR}/simulator
)

file(GLOB_RECURSE SOURCES
    "common/*.cpp"
    "rl_planning_infer/*.cpp"
    "data_collection/*.cc"
    "data_collection/*.cpp"
    "state_machine/*.cpp"
    "*.cpp"
    "simulator/*.cpp"
)
list(FILTER SOURCES EXCLUDE REGEX "main.cpp")
list(FILTER SOURCES EXCLUDE REGEX ".*test.*\\.cpp")
list(FILTER SOURCES EXCLUDE REGEX ".*_test\\.cpp")

# 打印源文件列表用于调试
foreach(f IN LISTS SOURCES)
    message(STATUS "Source file: ${f}")
endforeach()

# 创建共享库
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# 链接库文件
if(ENABLE_ROS2)
    find_package(ament_cmake REQUIRED)
    find_package(rclcpp REQUIRED)
    find_package(std_msgs REQUIRED)
    find_package(rosbag2_cpp REQUIRED)
    find_package(rosbag2_storage REQUIRED)
    find_package(rosbag2_storage_default_plugins REQUIRED)
    find_package(nav_msgs REQUIRED)
    find_package(sensor_msgs REQUIRED)
    find_package(geometry_msgs REQUIRED)
    find_package(tf2_msgs REQUIRED)
    find_package(visualization_msgs REQUIRED)
    find_package(rosidl_default_generators REQUIRED)

    if (0)
        ###################TODO FOR DELETE##################################
        include_directories(
                /opt/ros/humble/include
                /opt/ros/humble/include/rclcpp
                /opt/ros/humble/include/rosbag2_cpp
                /opt/ros/humble/include/rosbag2_storage
                /opt/ros/humble/include/rosidl_typesupport_introspection_cpp
                /opt/ros/humble/include/libstatistics_collector
                /opt/ros/humble/include/rcl
                /opt/ros/humble/include/rcutils
                /opt/ros/humble/include/rcl_yaml_param_parser
                /opt/ros/humble/include/rosidl_runtime_c
                /opt/ros/humble/include/rosidl_typesupport_interface
                /opt/ros/humble/include/rcpputils
                /opt/ros/humble/include/builtin_interfaces
                /opt/ros/humble/include/rmw
                /opt/ros/humble/include/rosidl_runtime_cpp
                /opt/ros/humble/include/tracetools
                /opt/ros/humble/include/rcl_interfaces
                /opt/ros/humble/include/statistics_msgs
                /opt/ros/humble/include/std_msgs
                /opt/ros/humble/include/nav_msgs
                /opt/ros/humble/include/sensor_msgs
                /opt/ros/humble/include/geometry_msgs
                /opt/ros/humble/include/action_msgs
                /opt/ros/humble/include/unique_identifier_msgs
                /opt/ros/humble/include/tf2_ros
                /opt/ros/humble/include/tf2_msgs
                /opt/ros/humble/include/visualization_msgs
        )

        link_directories(/opt/ros/humble/lib/)
        include_directories(${PROJECT_SOURCE_DIR}/build/src/rosidl_generator_cpp)
        ###################TODO FOR DELETE##################################
    endif ()

    # Add ROS2 include directory
    get_filename_component(ROS_BASE_DIR "${rclcpp_DIR}/../../../" ABSOLUTE)
    include_directories(SYSTEM "${ROS_BASE_DIR}/include")

    link_directories("${ROS_BASE_DIR}/lib")
    target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_BINARY_DIR}/src/rosidl_generator_cpp
        ${CMAKE_BINARY_DIR}/src/rosidl_generator_c
    )

    ament_target_dependencies(${PROJECT_NAME}
        rclcpp
        rcutils
        rosbag2_cpp
        rosbag2_storage
        rosbag2_storage_default_plugins
        std_msgs
        nav_msgs
        sensor_msgs
        geometry_msgs
        tf2_msgs
        visualization_msgs
    )

    target_link_libraries(${PROJECT_NAME}
        Boost::regex
        ruckig::ruckig
        ${THIRD_LIBRARIES}
    )

    # Generate ROS2 service interfaces
    rosidl_generate_interfaces(aurora_edge_interfaces
      "simulator/srv/SetTargetPath.srv"
      "simulator/srv/GetCurrentPosition.srv"
      "simulator/srv/GetErrorStatistics.srv"
      "simulator/srv/ClearErrors.srv"
      "data_collection/srv/TriggerRecording.srv"
      DEPENDENCIES geometry_msgs std_msgs nav_msgs
    )

endif()

set(EDGE_APP "aer")
add_executable(${EDGE_APP} main.cpp)
target_link_libraries(${EDGE_APP} PUBLIC
    ${PROJECT_NAME}
)

# Add include directories and link libraries
if(ENABLE_ROS2)
    target_include_directories(${EDGE_APP} PRIVATE
        ${CMAKE_BINARY_DIR}/src/rosidl_generator_cpp
        ${CMAKE_BINARY_DIR}/src/rosidl_generator_c
    )
    # Link against the generated interface libraries and ROS2 client library
    # Need both cpp and fastrtps typesupport
    if(TARGET aurora_edge_interfaces__rosidl_typesupport_cpp)
        target_link_libraries(${EDGE_APP} PRIVATE
            aurora_edge_interfaces__rosidl_typesupport_cpp
            rcl
        )
    endif()
    if(TARGET aurora_edge_interfaces__rosidl_typesupport_fastrtps_cpp)
        target_link_libraries(${EDGE_APP} PRIVATE
            aurora_edge_interfaces__rosidl_typesupport_fastrtps_cpp
        )
    endif()
endif()

# 创建Robot Simulator可执行文件
set(ROBOT_SIM_APP "robot_sim")
add_executable(${ROBOT_SIM_APP} simulator/robot_sim_main.cpp)
target_link_libraries(${ROBOT_SIM_APP} PUBLIC ${PROJECT_NAME})

# Add include directories and link libraries
if(ENABLE_ROS2)
    target_include_directories(${ROBOT_SIM_APP} PRIVATE
        ${CMAKE_BINARY_DIR}/src/rosidl_generator_cpp
        ${CMAKE_BINARY_DIR}/src/rosidl_generator_c
    )
    # Link against the generated interface libraries and ROS2 client library
    # Need both cpp and fastrtps typesupport
    if(TARGET aurora_edge_interfaces__rosidl_typesupport_cpp)
        target_link_libraries(${ROBOT_SIM_APP} PRIVATE
            aurora_edge_interfaces__rosidl_typesupport_cpp
            rcl
            tracetools
        )
    endif()
    if(TARGET aurora_edge_interfaces__rosidl_typesupport_fastrtps_cpp)
        target_link_libraries(${ROBOT_SIM_APP} PRIVATE
            aurora_edge_interfaces__rosidl_typesupport_fastrtps_cpp
        )
    endif()

    # Create symbolic links for runtime library lookup
    add_custom_target(runtime_symlinks ALL
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            libaurora_edge_interfaces__rosidl_typesupport_cpp.so
            ${CMAKE_BINARY_DIR}/src/libaurora_edge_runtime__rosidl_typesupport_cpp.so
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            libaurora_edge_interfaces__rosidl_typesupport_fastrtps_cpp.so
            ${CMAKE_BINARY_DIR}/src/libaurora_edge_runtime__rosidl_typesupport_fastrtps_cpp.so
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            libaurora_edge_interfaces__rosidl_typesupport_introspection_cpp.so
            ${CMAKE_BINARY_DIR}/src/libaurora_edge_runtime__rosidl_typesupport_introspection_cpp.so
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            libaurora_edge_interfaces__rosidl_typesupport_c.so
            ${CMAKE_BINARY_DIR}/src/libaurora_edge_runtime__rosidl_typesupport_c.so
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            libaurora_edge_interfaces__rosidl_typesupport_fastrtps_c.so
            ${CMAKE_BINARY_DIR}/src/libaurora_edge_runtime__rosidl_typesupport_fastrtps_c.so
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            libaurora_edge_interfaces__rosidl_typesupport_introspection_c.so
            ${CMAKE_BINARY_DIR}/src/libaurora_edge_runtime__rosidl_typesupport_introspection_c.so
        DEPENDS aurora_edge_interfaces
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/src
        COMMENT "Creating runtime library symlinks for ROS2 type support"
        VERBATIM
    )

endif()
add_dependencies(${ROBOT_SIM_APP} runtime_symlinks)
add_dependencies(${EDGE_APP} runtime_symlinks)


# 如果启用了ONNX Runtime，则链接相关库
if(HAVE_ONNXRUNTIME)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_ONNXRUNTIME)
    target_compile_definitions(${EDGE_APP} PRIVATE HAVE_ONNXRUNTIME)
    target_include_directories(${PROJECT_NAME} PRIVATE ${ONNXRUNTIME_PATH}/include)
    target_include_directories(${EDGE_APP} PRIVATE ${ONNXRUNTIME_PATH}/include)
    target_link_libraries(${PROJECT_NAME} ${ONNXRUNTIME_PATH}/lib/libonnxruntime.so)
    target_link_libraries(${EDGE_APP} PRIVATE ${ONNXRUNTIME_PATH}/lib/libonnxruntime.so)

    # 复制ONNX Runtime库文件到构建目录以便运行时链接
    file(GLOB ONNXRUNTIME_LIBS "${ONNXRUNTIME_PATH}/lib/*.so")
    file(COPY ${ONNXRUNTIME_LIBS} DESTINATION ${CMAKE_BINARY_DIR})
endif()


install(TARGETS ${EDGE_APP} ${ROBOT_SIM_APP} ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

if(BUILD_TESTS)
    # 基准测试可执行文件
    add_executable(benchmark_main ../tools/benchmark_main.cpp)
    target_include_directories(benchmark_main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(benchmark_main PUBLIC ${PROJECT_NAME})

    # 安装基准测试工具
    install(TARGETS benchmark_main
        RUNTIME DESTINATION bin
    )
endif()