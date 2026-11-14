# SPDX-License-Identifier: MIT
# Runs ../host_lfs1_test.c on qemu_cortex_m3 (no host C compiler needed):
#   west build -b qemu_cortex_m3 tests/qemu -d <dir> && timeout 20 west build -d <dir> -t run
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lfs1_reader_test)
set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/lfs1)
target_sources(app PRIVATE ../host_lfs1_test.c ${SRC}/lfs1.c ${SRC}/lfs1_util.c)
target_include_directories(app PRIVATE ${SRC})
set_source_files_properties(${SRC}/lfs1.c PROPERTIES COMPILE_OPTIONS "-Wno-sign-compare")
