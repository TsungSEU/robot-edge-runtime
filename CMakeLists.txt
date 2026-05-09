cmake_minimum_required(VERSION 3.22)

# Project version
set(MODULE_VERSION "1.10.3")

project(aurora_edge_runtime VERSION ${MODULE_VERSION})

# 编译器标志
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -fext-numeric-literals -pipe -O2 -fopenmp -fPIC -pthread -g")
add_compile_options(-Wall -Wextra -Wpedantic)

option(ENABLE_ROS2 "Enable ROS2 support" ON)
option(DOWNLOAD_ONNXRUNTIME "Download ONNX Runtime if not found" ON)

find_package(Boost CONFIG REQUIRED COMPONENTS filesystem system iostreams thread regex)
include_directories(${Boost_INCLUDE_DIRS})

include(cmake/Find3rdparty.cmake)

# ONNX Runtime支持
set(ONNXRUNTIME_PATH "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/onnxruntime")
set(CMAKE_PREFIX_PATH "${ONNXRUNTIME_PATH};${CMAKE_PREFIX_PATH}")

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty
)

# 检查ONNX Runtime是否可用或需要下载
if(NOT EXISTS "${ONNXRUNTIME_PATH}" AND DOWNLOAD_ONNXRUNTIME)
    message(STATUS "ONNX Runtime not found. Downloading...")

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
            set(ONNXRUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v1.10.0/onnxruntime-linux-x64-1.16.3.tgz")
            set(ONNXRUNTIME_DIR "onnxruntime-linux-x64-1.16.3")
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
            set(ONNXRUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v1.10.0/onnxruntime-linux-aarch64-1.16.3.tgz")
            set(ONNXRUNTIME_DIR "onnxruntime-linux-aarch64-1.16.3")
        endif()
    endif()

    # 下载并解压ONNX Runtime
    if(ONNXRUNTIME_URL)
        include(FetchContent)
        FetchContent_Populate(
            onnxruntime
            URL ${ONNXRUNTIME_URL}
            SOURCE_DIR ${ONNXRUNTIME_PATH}
        )
        message(STATUS "ONNX Runtime downloaded to ${ONNXRUNTIME_PATH}")
    endif()
endif()

# 尝试查找ONNX Runtime
if(EXISTS "${ONNXRUNTIME_PATH}")
    set(HAVE_ONNXRUNTIME TRUE)
    message(STATUS "Found ONNX Runtime: ${ONNXRUNTIME_PATH}")
endif()


# 获取所有链接目录
get_property(link_dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY LINK_DIRECTORIES)

# 打印所有链接目录
foreach(dir IN LISTS link_dirs)
    message(STATUS "Link directory: ${dir}")
endforeach()

# 获取Git commit hash用于启动日志
find_package(Git)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short=8 HEAD
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT GIT_COMMIT_HASH)
        set(GIT_COMMIT_HASH "unknown")
    endif()
else()
    set(GIT_COMMIT_HASH "unknown")
endif()

# 构建时间戳
string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S")

# 添加编译定义
add_compile_definitions(MODULE_VERSION="${MODULE_VERSION}")
add_compile_definitions(GIT_COMMIT_HASH="${GIT_COMMIT_HASH}")
add_compile_definitions(BUILD_TIMESTAMP="${BUILD_TIMESTAMP}")

message(STATUS "Build version: ${MODULE_VERSION}")
message(STATUS "Git commit: ${GIT_COMMIT_HASH}")
message(STATUS "Build timestamp: ${BUILD_TIMESTAMP}")

# Coverage support (must be before add_subdirectory)
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)
if(ENABLE_COVERAGE)
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

# 添加子目录
add_subdirectory(src)

# 添加测试（可选）
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
    enable_testing()
endif()

#############
##  CPack  ##
#############

set(CPACK_GENERATOR "TGZ")

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${MODULE_VERSION})
set(CPACK_PACKAGE_CONTACT "xucong@synaptix.ai")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_SET_DESTDIR ON)
set(CPACK_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_PACKAGE_FILE_NAME "${MODULE_VERSION}")


install(
        FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/resource/tar_extra/release_helper.sh
        DESTINATION /
)

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
include(CPack)