cmake_minimum_required(VERSION 3.15)

project(ruckig LANGUAGES CXX)

# 检测是否作为子目录使用（非ROS2模式）
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # 独立构建模式 - 需要ament_cmake
    find_package(ament_cmake REQUIRED)
    set(RUCKIG_STANDALONE TRUE)
else()
    # 子目录模式 - 不需要ROS2依赖
    set(RUCKIG_STANDALONE FALSE)
endif()

# 构建 core ruckig 库（静态或动态由 BUILD_SHARED_LIBS 控制）
add_library(
  ruckig
  src/ruckig/brake.cpp
  src/ruckig/position_first_step1.cpp
  src/ruckig/position_first_step2.cpp
  src/ruckig/position_second_step1.cpp
  src/ruckig/position_second_step2.cpp
  src/ruckig/position_third_step1.cpp
  src/ruckig/position_third_step2.cpp
  src/ruckig/velocity_second_step1.cpp
  src/ruckig/velocity_second_step2.cpp
  src/ruckig/velocity_third_step1.cpp
  src/ruckig/velocity_third_step2.cpp)

target_compile_features(ruckig PUBLIC cxx_std_17)

target_include_directories(
  ruckig
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>)

# 添加别名以便通过 ruckig::ruckig 使用
add_library(ruckig::ruckig ALIAS ruckig)

# 编译器警告屏蔽设置
if(MSVC)
  target_compile_definitions(ruckig PUBLIC _USE_MATH_DEFINES)
else()
  target_compile_options(ruckig PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# 仅在独立构建时使用ROS2安装规则
if(RUCKIG_STANDALONE)
    # 安装目标
    install(
      TARGETS ruckig
      EXPORT ruckig_targets
      LIBRARY DESTINATION lib
      ARCHIVE DESTINATION lib
      RUNTIME DESTINATION bin
      INCLUDES
      DESTINATION include)

    # 安装头文件
    install(
      DIRECTORY include/
      DESTINATION include
      FILES_MATCHING
      PATTERN "*.hpp")

    # 导出目标
    ament_export_targets(ruckig_targets HAS_LIBRARY_TARGET)
    ament_export_dependencies(ament_cmake)
    ament_export_include_directories(include)
    ament_export_libraries(ruckig)

    # 安装 CMake 配置文件
    ament_package()
endif()
