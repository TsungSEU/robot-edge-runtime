# Third-party dependencies for ad_shadowmode project

# set(SENSEAUTO_RSCL /opt/senseauto/senseauto-rscl/active)
# set(SENSEAUTO_MSG /opt/senseauto/senseauto-humanoid_interfaces/active)
set(TDR_3RDPARTY /opt/senseauto/senseauto-3rdparty/active/3rdparty)

# Include directories
include_directories(
    # ${SENSEAUTO_RSCL}
    # ${SENSEAUTO_RSCL}/include
    # ${SENSEAUTO_MSG}/include
    ${TDR_3RDPARTY}/boost/include
    ${TDR_3RDPARTY}/capnp/include
    ${TDR_3RDPARTY}/gflags/include
    ${TDR_3RDPARTY}/glog/include
    ${TDR_3RDPARTY}/gtest/include
    ${TDR_3RDPARTY}/jsoncpp/include
    ${TDR_3RDPARTY}/lz4/include
    ${TDR_3RDPARTY}/opencv/include
    ${TDR_3RDPARTY}/openssl/include
    ${TDR_3RDPARTY}/protobuf/include
    ${TDR_3RDPARTY}/uuid/include
    ${TDR_3RDPARTY}/yaml-cpp/include
    ${TDR_3RDPARTY}/zmq/include
    ${TDR_3RDPARTY}/zstd/include
    ${TDR_3RDPARTY}/curl/include
)

# Library files
# file(GLOB SENSEAUTO_RSCL_LIBS ${SENSEAUTO_RSCL}/lib/*.so ${SENSEAUTO_RSCL}/lib/*.a)
# file(GLOB SENSEAUTO_MSG_LIBS ${SENSEAUTO_MSG}/lib/*.so ${SENSEAUTO_MSG}/lib/*.a)

# Third-party libraries
# set(SENSEAUTO_LIBS
#     ${SENSEAUTO_RSCL_LIBS}
#     ${SENSEAUTO_MSG_LIBS}
# )
set(THIRD_PARTY_LIBS
    ${TDR_3RDPARTY}/curl/lib/libcurl.so
    ${TDR_3RDPARTY}/openssl/lib64/libssl.so
    ${TDR_3RDPARTY}/openssl/lib64/libcrypto.so
    ${TDR_3RDPARTY}/uuid/lib/libuuid.so
    ${TDR_3RDPARTY}/capnp/lib/libcapnp.so
    ${TDR_3RDPARTY}/lib/libkj.so
    ${TDR_3RDPARTY}/yaml-cpp/lib/libyaml-cpp.so
    ${TDR_3RDPARTY}/glog/lib/libglog.so
    ${TDR_3RDPARTY}/gflags/lib/libgflags.so
    ${TDR_3RDPARTY}/protobuf/lib/libprotobuf.so.25
    ${TDR_3RDPARTY}/zmq/lib/libzmq.so
    ${TDR_3RDPARTY}/zstd/lib/libzstd.so
    ${TDR_3RDPARTY}/lz4/lib/liblz4.so
)

# Boost libraries
set(BOOST_LIBS
    ${TDR_3RDPARTY}/boost/lib/libboost_filesystem.so
    ${TDR_3RDPARTY}/boost/lib/libboost_system.so
    ${TDR_3RDPARTY}/boost/lib/libboost_regex.so
)
