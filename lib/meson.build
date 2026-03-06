# Copyright (C) 2024-2026 Intel Corporation
# SPDX-License-Identifier: MIT

# Library sources
lib_sources = files(
  'c10.cpp',
  'c20.cpp',
  'combo.cpp',
  'cx0_helper.cpp',
  'debug.cpp',
  'dkl.cpp',
  'dp_m_n.cpp',
  'mmio_factory.cpp',
  'phy.cpp',
  'utils.cpp',
  'vsyncalter.cpp',
)

# Platform-specific OS sources
os_sources = []
if platform == 'linux'
  os_sources = files(
    '../os/linux/drm_platform_linux.cpp',
    '../os/linux/file_platform_linux.cpp',
    '../os/linux/mmio_linux.cpp',
    '../os/linux/network_platform_linux.cpp',
    '../os/linux/process_platform_linux.cpp',
    '../os/linux/system_platform_linux.cpp',
  )
endif

# All sources
all_sources = lib_sources + os_sources

# Library headers
lib_headers = files(
  'c10.h',
  'c20.h',
  'combo.h',
  'common.h',
  'cx0_helper.h',
  'dkl.h',
  'dp_m_n.h',
  'i915_pciids.h',
  'macros.h',
  'phy.h',
  'types.h',
  'utils.h',
)

# Library include directories
lib_inc = include_directories('.')
platforms_inc = include_directories('platforms')
os_inc = include_directories('../os')

# Additional includes based on platform
if platform == 'linux'
  lib_inc_dirs = [lib_inc, platforms_inc, os_inc, inc_dirs, drm_inc]
endif

# Code coverage support
lib_coverage_args = []
lib_coverage_link_args = []
if get_option('enable_coverage')
  lib_coverage_args = ['-fprofile-arcs', '-ftest-coverage', '-O0', '-g']
  lib_coverage_link_args = ['-fprofile-arcs', '-ftest-coverage']
endif

# Shared library (PIC is automatic for shared libraries in Meson)
vsyncalter_shared = shared_library('vsyncalter',
  all_sources,
  include_directories: lib_inc_dirs,
  dependencies: platform_deps,
  cpp_args: common_compile_args + platform_compile_args + lib_coverage_args,
  link_args: lib_coverage_link_args,
  version: meson.project_version(),
  soversion: version_major,
  install: true,
)

# Static library
vsyncalter_static = static_library('vsyncalter',
  all_sources,
  include_directories: lib_inc_dirs,
  dependencies: platform_deps,
  cpp_args: common_compile_args + platform_compile_args + lib_coverage_args,
  link_args: lib_coverage_link_args,
  install: true,
)

# Create dependency objects for use in other subprojects
vsyncalter_shared_dep = declare_dependency(
  link_with: vsyncalter_shared,
  include_directories: lib_inc_dirs,
  dependencies: platform_deps,
)

vsyncalter_static_dep = declare_dependency(
  link_with: vsyncalter_static,
  include_directories: lib_inc_dirs,
  dependencies: platform_deps,
)

# Install headers
install_headers(lib_headers,
  subdir: 'vsyncalter'
)
