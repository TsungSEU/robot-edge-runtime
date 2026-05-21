## 3rdparty lib
SET(THIRD_DEPS_DIR "${CMAKE_SOURCE_DIR}/3rdparty" CACHE PATH "Third-party dependency root")

# YAML-CPP
SET(YAML_CPP_INCLUDE_DIR ${THIRD_DEPS_DIR}/yaml-cpp/include CACHE INTERNAL "yaml-cpp include dir.")
SET(YAML_CPP_LIBRARY_DIR ${THIRD_DEPS_DIR}/yaml-cpp/lib CACHE INTERNAL "yaml-cpp library dir.")
SET(YAML_CPP_LIBRARIES ${YAML_CPP_LIBRARY_DIR}/libyaml-cpp.so CACHE INTERNAL "yaml-cpp libraries.")
message(STATUS "Found yaml-cpp: ${THIRD_DEPS_DIR}/yaml-cpp")

# ThreadPool
SET(THREADPOOL_INCLUDE_DIR ${THIRD_DEPS_DIR}/ThreadPool CACHE INTERNAL "threadpool include dir.")
message(STATUS "Found Threadpool: ${THIRD_DEPS_DIR}/ThreadPool")

# AWS-SDK
SET(AWS_SDK_INCLUDE_DIR ${THIRD_DEPS_DIR}/aws-sdk/x86/include CACHE INTERNAL "aws sdk include dir.")
SET(AWS_SDK_LIBRARY_DIR ${THIRD_DEPS_DIR}/aws-sdk/x86/lib CACHE INTERNAL "aws sdk library dir.")
SET(AWS_SDK_LIBRARIES 
    ${AWS_SDK_LIBRARY_DIR}/libaws-cpp-sdk-core.so
    ${AWS_SDK_LIBRARY_DIR}/libaws-cpp-sdk-s3.so
    ${AWS_SDK_LIBRARY_DIR}/libaws-crt-cpp.so
    ${AWS_SDK_LIBRARY_DIR}/libaws-c-s3.so
    ${AWS_SDK_LIBRARY_DIR}/libaws-c-sdkutils.so
    ${AWS_SDK_LIBRARY_DIR}/libaws-checksums.so
    ${AWS_SDK_LIBRARY_DIR}/libaws-crt-cpp.so
    ${AWS_SDK_LIBRARY_DIR}/libaws-c-common.so
    ${AWS_SDK_LIBRARY_DIR}/libaws-c-auth.so
    ${AWS_SDK_LIBRARY_DIR}/libaws-c-http.so
    ${AWS_SDK_LIBRARY_DIR}/libaws-c-cal.so
    ${AWS_SDK_LIBRARY_DIR}/libaws-c-compression.so
    ${AWS_SDK_LIBRARY_DIR}/libaws-c-event-stream.so
    ${AWS_SDK_LIBRARY_DIR}/libaws-c-io.so
    ${AWS_SDK_LIBRARY_DIR}/libaws-c-mqtt.so
    ${AWS_SDK_LIBRARY_DIR}/libs2n.so CACHE INTERNAL "aws sdk libraries.")
message(STATUS "Found aws-sdk: ${THIRD_DEPS_DIR}/aws-sdk")

# curl
SET(CURL_INCLUDE_DIR ${THIRD_DEPS_DIR}/curl/x86/include CACHE INTERNAL "curl include dir.")
SET(CURL_LIBRARY_DIR ${THIRD_DEPS_DIR}/curl/x86/lib CACHE INTERNAL "curl library dir.")
SET(CURL_LIBRARIES ${CURL_LIBRARY_DIR}/libcurl.so CACHE INTERNAL "curl libraries.")
message(STATUS "Found curl: ${THIRD_DEPS_DIR}/curl")

# FFmpeg
SET(FFMPEG_INCLUDE_DIR ${THIRD_DEPS_DIR}/ffmpeg/x86/include CACHE INTERNAL "ffmpeg include dir.")
SET(FFMPEG_LIBRARY_DIR ${THIRD_DEPS_DIR}/ffmpeg/x86/lib CACHE INTERNAL "ffmpeg library dir.")
SET(FFMPEG_LIBRARIES
    ${FFMPEG_LIBRARY_DIR}/libavahi-client.so.3
    ${FFMPEG_LIBRARY_DIR}/libavahi-common.so.3
    ${FFMPEG_LIBRARY_DIR}/libavc1394.so.0
    ${FFMPEG_LIBRARY_DIR}/libavcodec.so
    ${FFMPEG_LIBRARY_DIR}/libavcodec-ffmpeg.so
    ${FFMPEG_LIBRARY_DIR}/libavdevice-ffmpeg.so
    ${FFMPEG_LIBRARY_DIR}/libavfilter-ffmpeg.so
    ${FFMPEG_LIBRARY_DIR}/libavformat.so
    ${FFMPEG_LIBRARY_DIR}/libavresample.so
    ${FFMPEG_LIBRARY_DIR}/libavutil.so
    ${FFMPEG_LIBRARY_DIR}/libpostproc-ffmpeg.so
    ${FFMPEG_LIBRARY_DIR}/libswresample-ffmpeg.so
    ${FFMPEG_LIBRARY_DIR}/libswscale.so
    ${FFMPEG_LIBRARY_DIR}/libswscale-ffmpeg.so CACHE INTERNAL "ffmpeg libraries.")
message(STATUS "Found ffmpeg: ${THIRD_DEPS_DIR}/ffmpeg")

# LZ4
SET(LZ4_INCLUDE_DIR ${THIRD_DEPS_DIR}/lz4/x86/include CACHE INTERNAL "lz4 include dir.")
SET(LZ4_LIBRARY_DIR ${THIRD_DEPS_DIR}/lz4/x86/lib CACHE INTERNAL "lz4 library dir.")
SET(LZ4_LIBRARIES ${LZ4_LIBRARY_DIR}/liblz4.so CACHE INTERNAL "lz4 libraries.")
message(STATUS "Found lz4: ${THIRD_DEPS_DIR}/lz4")

# zlib (system library)
find_package(ZLIB REQUIRED)
message(STATUS "Found zlib: ${ZLIB_LIBRARIES}")

# Manif
SET(MANIF_INCLUDE_DIR ${THIRD_DEPS_DIR}/manif CACHE INTERNAL "manif include dir.")
message(STATUS "Found manif: ${THIRD_DEPS_DIR}/manif")

# Microtar
add_library(microtar STATIC ${THIRD_DEPS_DIR}/microtar/microtar.c)
SET(MICROTAR_INCLUDE_DIR ${THIRD_DEPS_DIR}/microtar CACHE INTERNAL "microtar include dir.")
SET(MICROTAR_LIBRARIES microtar CACHE INTERNAL "microtar libraries.")
message(STATUS "Found microtar: ${THIRD_DEPS_DIR}/microtar")

# Nlohmann JSON
SET(NLOHMANN_JSON_INCLUDE_DIR ${THIRD_DEPS_DIR}/nlohmann CACHE INTERNAL "nlohmann json include dir.")
message(STATUS "Found nlohmann: ${THIRD_DEPS_DIR}/nlohmann")

# ONNXRuntime
SET(ONNXRUNTIME_INCLUDE_DIR ${THIRD_DEPS_DIR}/onnxruntime/include CACHE INTERNAL "onnxruntime include dir.")
SET(ONNXRUNTIME_LIBRARY_DIR ${THIRD_DEPS_DIR}/onnxruntime/lib CACHE INTERNAL "onnxruntime library dir.")
SET(ONNXRUNTIME_LIBRARIES ${ONNXRUNTIME_LIBRARY_DIR}/libonnxruntime.so CACHE INTERNAL "onnxruntime libraries.")
message(STATUS "Found onnxruntime: ${THIRD_DEPS_DIR}/onnxruntime")

# OpenSSL
SET(OPENSSL_ROOT_DIR ${THIRD_DEPS_DIR}/openssl/x86 CACHE INTERNAL "openssl root dir.")
SET(OPENSSL_INCLUDE_DIR ${THIRD_DEPS_DIR}/openssl/x86/include CACHE INTERNAL "openssl include dir.")
SET(OPENSSL_LIBRARY_DIR ${THIRD_DEPS_DIR}/openssl/x86/lib CACHE INTERNAL "openssl library dir.")
SET(OPENSSL_LIBRARIES
    ${OPENSSL_LIBRARY_DIR}/libssl.so
    ${OPENSSL_LIBRARY_DIR}/libcrypto.so CACHE INTERNAL "openssl libraries.")
message(STATUS "Found openssl: ${THIRD_DEPS_DIR}/openssl")

# Paho-MQTT-CPP
SET(PAHO_MQTT_CPP_INCLUDE_DIR ${THIRD_DEPS_DIR}/paho.mqtt.cpp/x86/include CACHE INTERNAL "paho mqtt cpp include dir.")
SET(PAHO_MQTT_CPP_LIBRARY_DIR ${THIRD_DEPS_DIR}/paho.mqtt.cpp/x86/lib CACHE INTERNAL "paho mqtt cpp library dir.")
SET(PAHO_MQTT_CPP_LIBRARIES
    ${PAHO_MQTT_CPP_LIBRARY_DIR}/libpaho-mqtt3cs.so
    ${PAHO_MQTT_CPP_LIBRARY_DIR}/libpaho-mqtt3as.so
    ${PAHO_MQTT_CPP_LIBRARY_DIR}/libpaho-mqttpp3.so CACHE INTERNAL "paho mqtt cpp libraries.")
message(STATUS "Found paho-mqtt-cpp: ${THIRD_DEPS_DIR}/paho.mqtt.cpp")

# TL
SET(TL_INCLUDE_DIR ${THIRD_DEPS_DIR}/tl/include CACHE INTERNAL "tl include dir.")
message(STATUS "Found Tl: ${THIRD_DEPS_DIR}/tl")

# Exprtk
SET(EXPRTK_INCLUDE_DIR ${THIRD_DEPS_DIR}/exprtk CACHE INTERNAL "exprtk include dir.")
message(STATUS "Found exprtk: ${THIRD_DEPS_DIR}/exprtk")

# Eigen3 (header-only library)
SET(EIGEN3_INCLUDE_DIR ${THIRD_DEPS_DIR}/Eigen CACHE INTERNAL "eigen3 include dir.")
message(STATUS "Found Eigen3: ${THIRD_DEPS_DIR}/Eigen")

# ruckig
add_subdirectory(${THIRD_DEPS_DIR}/ruckig)

include_directories(${YAML_CPP_INCLUDE_DIR} ${THREADPOOL_INCLUDE_DIR} ${AWS_SDK_INCLUDE_DIR} ${CURL_INCLUDE_DIR} ${FFMPEG_INCLUDE_DIR} ${LZ4_INCLUDE_DIR} ${MANIF_INCLUDE_DIR} ${MICROTAR_INCLUDE_DIR} ${NLOHMANN_JSON_INCLUDE_DIR} ${ONNXRUNTIME_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR} ${PAHO_MQTT_CPP_INCLUDE_DIR} ${TL_INCLUDE_DIR} ${EXPRTK_INCLUDE_DIR} ${EIGEN3_INCLUDE_DIR} ${ZLIB_INCLUDE_DIRS})
list(APPEND THIRD_LIBRARIES ${YAML_CPP_LIBRARIES} ${AWS_SDK_LIBRARIES} ${CURL_LIBRARIES} ${FFMPEG_LIBRARIES} ${LZ4_LIBRARIES} ${MICROTAR_LIBRARIES} ${ONNXRUNTIME_LIBRARIES} ${OPENSSL_LIBRARIES} ${PAHO_MQTT_CPP_LIBRARIES} ZLIB::ZLIB)