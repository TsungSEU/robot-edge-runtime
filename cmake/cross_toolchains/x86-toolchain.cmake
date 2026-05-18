set(BUILDTOOLS_TARGET_PLATFORM "x86_64")

#######################
#    Project Config
#######################

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_SYSROOT "") # sysroot will be stripped from rpath, so empty but means for "/"
# set(CMAKE_STAGING_PREFIX /opt/orderseek/stoic/staging/x86_64) # path on the host to install to

set(CMAKE_C_COMPILER /usr/bin/gcc)
set(CMAKE_CXX_COMPILER /usr/bin/g++)

set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CUDA_TOOLKIT_ROOT_DIR /usr/local/cuda)
set(CUDA_TOOLKIT_INCLUDE ${CUDA_TOOLKIT_ROOT_DIR}/include)
set(CUDA_INCLUDE_DIRS ${CUDA_TOOLKIT_ROOT_DIR}/include)
set(CUDA_CUDART_LIBRARY ${CUDA_TOOLKIT_ROOT_DIR}/lib64/libcudart.so)
set(CUDA_cublas_LIBRARY ${CUDA_TOOLKIT_ROOT_DIR}/lib64/libcublas.so)
set(CUDA_curand_LIBRARY ${CUDA_TOOLKIT_ROOT_DIR}/lib64/libcurand.so)
