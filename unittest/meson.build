project('vsyncalter_unittest', 'c', 'cpp',
  version: '1.0.0',
  default_options: ['c_std=c99', 'cpp_std=c++11']
)

fs = import('fs')
cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

# Platform detection
platform = host_machine.system()
if platform != 'linux'
  error('Unsupported platform')
endif

# Find dependencies
rt_dep = cc.find_library('rt', required: true)
drm_dep = dependency('libdrm', required: true)
pciaccess_dep = dependency('pciaccess', required: true)
threads_dep = dependency('threads', required: true)

platform_deps = [rt_dep, drm_dep, pciaccess_dep, threads_dep]

# Include directories
drm_inc = include_directories('/usr/include/drm', is_system: true)
app_inc_dirs = include_directories('../cmn', '../os')
unity_inc = include_directories('unity/src')

# Import the shared library
vsyncalter_lib = cpp.find_library('vsyncalter',
  dirs: [meson.source_root() + '/../builddir/lib'],
  required: true
)

# Unity and test sources - compiled as C++
# Note: using .cpp extension so Meson compiles them as C++
# This is needed because vsyncalter.h includes version.h which has C++ headers
test_genlock_cpp = custom_target('test_genlock_cpp',
  input: 'test_genlock.c',
  output: 'test_genlock.cpp',
  command: ['cp', '@INPUT@', '@OUTPUT@'],
)

unity_cpp = custom_target('unity_cpp',
  input: 'unity/src/unity.c',
  output: 'unity.cpp',
  command: ['cp', '@INPUT@', '@OUTPUT@'],
)

unittest_sources = [test_genlock_cpp, unity_cpp]

unittest_cpp_args = ['-Wall', '-fprofile-arcs', '-ftest-coverage', '-O0', '-g']
unittest_link_args = ['-fprofile-arcs', '-ftest-coverage']

# Set library path for runtime
unittest_exe = executable('swgenlock_tests',
  unittest_sources,
  include_directories: [app_inc_dirs, unity_inc, drm_inc],
  dependencies: [vsyncalter_lib, platform_deps],
  cpp_args: unittest_cpp_args,
  link_args: unittest_link_args,
  override_options: ['cpp_std=c++11'],
  install: false,
  build_rpath: meson.source_root() + '/../builddir/lib',
)
