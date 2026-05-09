#!/bin/bash

# release_helper.sh - 用于生成 release.conf.json

PROJECT_ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$PROJECT_ROOT"

RELEASE_CONF="release.conf.json"
PROJECT_NAME=${PROJECT_NAME:-"aurora-robot_edge"}
TARGET_PALTFORM=${TARGET_PALTFORM:-"thor"}
TGZ_VER="1.$(date '+%Y%m%d-%H%M%S')-$(git rev-parse --short=5 HEAD).${PROJECT_NAME}-${TARGET_PALTFORM}-dev"
CLEAN_VER=$([[ -n $(git status -s) ]] && echo '-dirty' || echo '-new')

cat > ${RELEASE_CONF} << EOF
{
  "MODULE_VERSION": "${TGZ_VER}${CLEAN_VER}",
  "BUILDTOOLS_TARGET_PLATFORM": "${TARGET_PALTFORM}",
  "PROJECT_NAME": "${PROJECT_NAME}",
  "AD_CROSS_COMPILE": "ON",
  "BT_CROSS_ROOT": "/usr/local/thor/",
  "BT_CROSS_TOOLCHAIN": "/usr/local/thor/aarch64--glibc--bleeding-edge-2024.02-1",
  "BT_SYSROOT": "/",
  "SENSEAUTO_DEV_INSTALLED_ROOT": "/",
  "CMAKE_SYSTEM_NAME": "Linux",
  "CMAKE_SYSTEM_PROCESSOR": "aarch64",
  "HOST_OS": "Linux",
  "ARCH_TYPE": "aarch64",
  "PLATFORM_NAME": "aarch64",
  "CPACK_GENERATOR": "TGZ",
  "INSTALL_SOFTLINK_SRC": ""
}
EOF
