# Copyright (C) 2024-2026 Intel Corporation
# SPDX-License-Identifier: MIT

project('vsyncalter', 'cpp',
  version: '4.0.0',
  license: 'MIT',
  default_options: [
    'cpp_std=c++11',
    'warning_level=2',
    'buildtype=release',
  ],
  meson_version: '>=0.56.0'
)

# Project information
project_description = 'VSync Alteration Library and Tools'

# Get version components
version_array = meson.project_version().split('.')
version_major = version_array[0]
version_minor = version_array[1]
version_patch = version_array[2]

# Platform detection
host_system = host_machine.system()

if host_system == 'linux'
  platform = 'linux'
  message('Detected platform: Linux')
else
  error('Unsupported platform: ' + host_system)
endif

# Build options
build_shared = get_option('build_shared_libs')
build_swgenlock = get_option('build_swgenlock')
build_pllctl = get_option('build_pllctl')
build_vblmon = get_option('build_vblmon')
build_framesync = get_option('build_framesync')
enable_coverage = get_option('enable_coverage')

# Compiler
cpp_compiler = meson.get_compiler('cpp')

# Global include directories
inc_dirs = include_directories('cmn', 'os', 'lib')
# App include dirs - only need cmn and os, no lib dependency
app_inc_dirs = include_directories('cmn', 'os')

# Platform-specific dependencies and settings
platform_deps = []
platform_link_args = []
platform_compile_args = []

if platform == 'linux'
  # Linux-specific includes
  drm_inc = include_directories('/usr/include/drm', is_system: true)

  # Find required libraries
  rt_dep = cpp_compiler.find_library('rt', required: true)
  drm_dep = dependency('libdrm', required: true)
  pciaccess_dep = dependency('pciaccess', required: true)
  threads_dep = dependency('threads', required: true)

  platform_deps = [rt_dep, drm_dep, pciaccess_dep, threads_dep]

  message('Linux libraries found:')
  message('  - rt: @0@'.format(rt_dep.found()))
  message('  - drm: @0@'.format(drm_dep.found()))
  message('  - pciaccess: @0@'.format(pciaccess_dep.found()))
  message('  - threads: @0@'.format(threads_dep.found()))

endif

# Common compile arguments
common_compile_args = ['-Wall']

if get_option('buildtype') == 'debug'
  common_compile_args += ['-g', '-O0', '-DDEBUGGON']
endif

# Subdirectories
subdir('lib')

if build_pllctl
  subdir('pllctl')
endif

if build_vblmon
  subdir('vblmon')
endif

if build_swgenlock
  subdir('swgenlock')
endif

if build_framesync
  subdir('framesync')
endif

# Installation of headers
install_subdir('cmn',
  install_dir: get_option('includedir') / 'vsyncalter',
  strip_directory: true,
  exclude_files: ['*.cpp', '*.o']
)

install_subdir('os',
  install_dir: get_option('includedir') / 'vsyncalter' / 'os',
  strip_directory: false,
  exclude_files: ['*.cpp', '*.o']
)

# pkg-config file generation
pkg = import('pkgconfig')
pkg.generate(
  vsyncalter_shared,
  name: 'vsyncalter',
  description: project_description,
  version: meson.project_version(),
  filebase: 'vsyncalter',
  subdirs: ['vsyncalter'],
)

# Print build configuration summary
summary_info = {
  'Version': meson.project_version(),
  'Platform': platform,
  'Build type': get_option('buildtype'),
  'C++ standard': get_option('cpp_std'),
  'Warning level': get_option('warning_level'),
  'Build framesync': build_framesync,
  'Shared libraries': build_shared,
  'Build pllctl': build_pllctl,
  'Build vblmon': build_vblmon,
  'Build swgenlock': build_swgenlock,
  'Coverage enabled': enable_coverage,
  'Install prefix': get_option('prefix'),
  'Include directory': get_option('includedir'),
  'Library directory': get_option('libdir'),
  'Binary directory': get_option('bindir'),
}

message('')
message('==================================================')
message('VSync Alteration Build Configuration Summary')
message('==================================================')
foreach key, value : summary_info
  message('  @0@: @1@'.format(key, value))
endforeach
message('==================================================')
message('')
