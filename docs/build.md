## Build steps

SW Genlock supports two build systems:
- **Meson** (recommended for speed and simplicity)
- **CMake** (cross-platform, Visual Studio support)

### Option 1: Meson Build (Recommended - Fast & Simple)

Meson provides the fastest build times with clean syntax and excellent cross-platform support.

1) Git clone this repository
2) Install dependencies:
```bash
sudo apt install libdrm-dev libpciaccess-dev meson ninja-build
```
   - If the directory /usr/include/drm does not exist, run: `sudo apt install -y linux-libc-dev`

3) Build using the provided script:
```bash
./build_meson.sh           # Default: Release build
./build_meson.sh debug     # For debug build
./build_meson.sh release clean  # Clean build
```

Or manually:
```bash
meson setup builddir
meson compile -C builddir
```

**Build outputs** will be in:
- `builddir/lib/` - libvsyncalter.so and libvsyncalter.a
- `builddir/pllctl/` - pllctl executable
- `builddir/vblmon/` - vblmon executable
- `builddir/swgenlock/` - swgenlock executable

For detailed Meson documentation, see [MESON_BUILD.md](MESON_BUILD.md).

### Option 2: CMake Build (Cross-Platform)

CMake provides cross-platform support, automatic platform detection, and Visual Studio integration for Windows development.

1) Git clone this repository
2) Install dependencies:
```bash
sudo apt install libdrm-dev libpciaccess-dev cmake
```
   - If the directory /usr/include/drm does not exist, run: `sudo apt install -y linux-libc-dev`

3) Build using the provided script:
```bash
./build_cmake.sh           # Default: Release build
./build_cmake.sh Debug     # For debug build
```

Or manually:
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

**Build outputs** will be in:
- `build/lib/` - libvsyncalter.so and libvsyncalter.a
- `build/pllctl/` - pllctl executable
- `build/vblmon/` - vblmon executable
- `build/swgenlock/` - swgenlock executable

**CMake options** (configure with `-D` flags):
- `BUILD_SHARED_LIBS=ON/OFF` - Build shared library (default: ON)
- `BUILD_SWGENLOCK=ON/OFF` - Build swgenlock applications (default: ON)
- `BUILD_PLLCTL=ON/OFF` - Build pllctl (default: ON)
- `BUILD_VBLMON=ON/OFF` - Build vblmon (default: ON)

Example: `cmake -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Debug ..`

# Software Components

## Vsync Library

The Vsync library is distributed in both dynamic (.so) and static (.a) binary formats, allowing users to choose between dynamic and static linking depending on their application requirements. This library includes essential functions such as retrieving vblank timestamps and adjusting the vblank timestamp drift by specified microseconds. Additionally, all operations related to Phase-Locked Loop (PLL) programming are encapsulated within the library, providing a comprehensive suite of tools for managing display synchronization.

### 🔧 Static Library (.a)
When linking against the static library (libvsyncalter.a), the final binary must be linked using g++ rather than gcc. This is because the static library contains C++ object code but does not include the C++ standard library symbols (e.g., operator new, std::mutex, virtual tables).

```bash
$ g++ -o app main.c libvsyncalter.a -lpciaccess -ldrm -lrt -lm
```

Linking with gcc in this case will typically result in undefined reference errors to C++ runtime symbols.

### 🔧 Shared Library (.so)
When using the shared library version (libvsyncalter.so), the final binary can be linked using either gcc or g++. All necessary C++ symbols are already resolved and included in the shared object.

The system dynamic linker will load the required C++ runtime (libstdc++) at runtime.

```bash
$ gcc -o app main.c -L. -lvsyncalter -lpciaccess -ldrm -lrt -lm
```

This allows the shared library to be used safely from C code, since the C++ functions in vsync lib are exposed with C-compatible linkage (i.e., using extern "C" in headers). The reference applications are linked using g++, while the unit tests are compiled with gcc to ensure compatibility with the pure C interface.

## Code Coverage Support

The library supports code coverage instrumentation for unit testing purposes. Code coverage is disabled by default in normal builds but can be enabled when running unit tests.

**CMake:** Use `-DUNITTEST_ENABLE_COVERAGE=ON` to enable code coverage
**Meson:** Use `-Denable_coverage=true` to enable code coverage

The unit test build scripts (`unittest/build_cmake.sh` and `unittest/build_meson.sh`) automatically enable code coverage.

**Note:** The build scripts automatically handle build configuration to ensure:
- When building for unit tests: code coverage is enabled
- When building normally from the root: code coverage is disabled

If a build directory already exists with the opposite configuration, the scripts will automatically reconfigure it. Alternatively, you can manually delete the build directory to start fresh:
```bash
rm -rf build        # For CMake
rm -rf builddir     # For Meson
```

For detailed unit testing and code coverage instructions, see [unittest/unittest.md](../unittest/unittest.md).
