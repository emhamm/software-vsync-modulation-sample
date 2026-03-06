# Copyright (C) 2024-2026 Intel Corporation
# SPDX-License-Identifier: MIT

# Check if building as part of main project or standalone
# When included via subdir() from main project, vsyncalter_static will be defined
is_standalone = not meson.is_subproject() and meson.project_name() != 'vsyncalter'

if is_standalone
  # Standalone build
  project('vblmon', 'cpp',
    version : '4.0.0',
    default_options : ['cpp_std=c++11', 'warning_level=2']
  )

  # Detect platform
  host_system = host_machine.system()
  if host_system == 'linux'
    platform = 'linux'
  else
    error('Unsupported platform: ' + host_system)
  endif

  # Find vsyncalter library
  vsyncalter_dep = dependency('vsyncalter', required: true)

  # Platform dependencies
  cpp_compiler = meson.get_compiler('cpp')
  rt_dep = cpp_compiler.find_library('rt', required: true)
  drm_dep = dependency('libdrm', required: true)
  pciaccess_dep = dependency('pciaccess', required: true)
  threads_dep = dependency('threads', required: true)
  platform_deps = [rt_dep, drm_dep, pciaccess_dep, threads_dep]

  drm_inc = include_directories('/usr/include/drm', is_system: true)
  app_inc_dirs = include_directories('/usr/local/include/vsyncalter', '/usr/include/vsyncalter')
  common_compile_args = ['-Wall']
  platform_compile_args = []
endif

# Vblmon sources
vblmon_sources = files(
  'vblmon.cpp',
)

# Platform-specific OS sources (needed for integrated build)
vblmon_os_sources = []
if is_standalone and platform == 'linux'
  # Standalone build - don't include OS sources (linked in library)
  vblmon_os_sources = []
elif not is_standalone and platform == 'linux'
  # Integrated build - include OS sources
  vblmon_os_sources = files(
    '../os/linux/drm_platform_linux.cpp',
    '../os/linux/file_platform_linux.cpp',
    '../os/linux/mmio_linux.cpp',
    '../os/linux/network_platform_linux.cpp',
    '../os/linux/process_platform_linux.cpp',
    '../os/linux/system_platform_linux.cpp',
  )
endif

# Include directories
if platform == 'linux'
  vblmon_inc_dirs = [app_inc_dirs, drm_inc]
endif

# Code coverage support
vblmon_coverage_args = []
vblmon_coverage_link_args = []
if get_option('enable_coverage')
  vblmon_coverage_args = ['-fprofile-arcs', '-ftest-coverage', '-O0', '-g']
  vblmon_coverage_link_args = ['-fprofile-arcs', '-ftest-coverage']
endif

# Create executable
if is_standalone
  # Standalone build
  vblmon_exe = executable('vblmon',
    vblmon_sources,
    include_directories: [app_inc_dirs, drm_inc],
    dependencies: [platform_deps, vsyncalter_dep],
    cpp_args: common_compile_args + platform_compile_args,
    install: true,
  )
else
  # Building as part of main project
  vblmon_exe = executable('vblmon',
    vblmon_sources + vblmon_os_sources,
    include_directories: vblmon_inc_dirs,
    dependencies: platform_deps,
    link_with: vsyncalter_static,
    cpp_args: common_compile_args + platform_compile_args + vblmon_coverage_args,
    link_args: vblmon_coverage_link_args,
    install: true,
  )
endif
