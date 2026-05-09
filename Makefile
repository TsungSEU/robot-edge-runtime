SHELL := /bin/bash

MW ?= "ROS2"
PROJECT_NAME ?= "aurora_robot"
PROJECT_ID ?= "robot"
TARGET_PALTFORM ?= "x86-64"
TGZ_VER := 1.$$(date '+%Y%m%d-%H%M%S')-$$(git rev-parse --short=5 HEAD).$(PROJECT_NAME)-$(TARGET_PALTFORM)-dev
CLEAN_VER := $$([[ -n $$(git status -s) ]] && echo '-dirty' || echo '-new')
JOBS ?= 8
RELEASE_CONF := release.conf.json

COLOR_RED := \033[1;31m
COLOR_GREEN := \033[1;32m
COLOR_YELLOW := \033[1;33m
COLOR_RESET := \033[0m

FORCE:

all: $(RELEASE_CONF)
	@mkdir -p build
	@echo ""
	@echo "###################################################"
	@echo "# CMake"
	@echo "###################################################"
	@echo ""
	@cd build && cmake ..
	@echo ""
	@echo "###################################################"
	@echo "# Build"
	@echo "###################################################"
	@echo ""
	@cd build && make --no-print-directory -j$(JOBS)

clean:
	@rm -r build
	@rm -f $(RELEASE_CONF)

# 生成 release.conf.json
$(RELEASE_CONF): FORCE
	@echo "Generating $@"
	@chmod +x ./resource/tar_extra/release_helper.sh
	@PROJECT_NAME="$(PROJECT_NAME)" TARGET_PALTFORM="$(TARGET_PALTFORM)" ./resource/tar_extra/release_helper.sh

tgz_thor: $(RELEASE_CONF)
	@echo "###################################################"
	@echo "# Make tgz_thor"
	@echo "###################################################"
	@mkdir -p build
	@cd build && cmake .. \
		-DCMAKE_TOOLCHAIN_FILE=./cmake/cross_toolchains/thor-toolchain.cmake \
		-DCMAKE_MW="$(MW)" \
		-DCMAKE_BUILD_TYPE="Release" \
		-DBUILD_THOR=ON \
		-DCMAKE_INSTALL_PREFIX="/opt/tmp/dcp" \
		-DMODULE_VERSION="$(TGZ_VER)$(CLEAN_VER)"
	@cd build && make package --no-print-directory -j$(JOBS)

tgz_orin: $(RELEASE_CONF)
	@echo "###################################################"
	@echo "# Make tgz_orin"
	@echo "###################################################"
	@mkdir -p build
	@cd build && cmake .. \
		-DCMAKE_TOOLCHAIN_FILE=./cmake/cross_toolchains/orin-toolchain.cmake \
		-DCMAKE_MW="$(MW)" \
		-DCMAKE_BUILD_TYPE="Release" \
		-DBUILD_ORIN=ON \
		-DCMAKE_INSTALL_PREFIX="/opt/tmp/dcp" \
		-DMODULE_VERSION="$(TGZ_VER)$(CLEAN_VER)"
	@cd build && make package --no-print-directory -j$(JOBS)

tgz_x86: $(RELEASE_CONF)
	@echo "###################################################"
	@echo "# Make tgz_x86"
	@echo "###################################################"
	@mkdir -p build
	@cd build && cmake .. \
		-DCMAKE_TOOLCHAIN_FILE=./cmake/cross_toolchains/x86-toolchain.cmake \
		-DCMAKE_BUILD_TYPE="Release" \
		-DCMAKE_INSTALL_PREFIX="/opt/tmp/dcp" \
		-DMODULE_VERSION="$(TGZ_VER)$(CLEAN_VER)"
	@cd build && make package --no-print-directory -j$(JOBS)
