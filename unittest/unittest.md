Unit Testing and Code Coverage Guide
====================================

This document outlines the procedure for executing unit tests and generating code coverage reports for the **SW GenLock library**, using the Unity C test framework in combination with `gcov` and `lcov`.

Setup Instructions
------------------

**Clone Unity Test Framework (Optional)**

The build scripts (`build_cmake.sh` and `build_meson.sh`) automatically clone Unity if not present. However, if you prefer to build manually using cmake or meson commands directly, clone the Unity test framework first:

```console
   $ cd unittest
   $ git clone https://github.com/ThrowTheSwitch/Unity.git unity --branch v2.6.1 --depth 1
```

This places `unity.c` and `unity.h` under `unittest/unity/src`.

Build the Unit Tests
--------------------

The unittest directory includes build scripts for both CMake and Meson build systems. These scripts automatically clone the Unity test framework if not present and build the library with coverage support enabled.

**Note:** The build scripts automatically enable code coverage instrumentation for the **vsyncalter library** by passing the appropriate build flags:
- CMake: `-DUNITTEST_ENABLE_COVERAGE=ON`
- Meson: `-Denable_coverage=true`

This ensures that the library source code is instrumented with `gcov` flags (`-fprofile-arcs -ftest-coverage`) for accurate coverage reporting.

**Important for Meson builds:** If the library was previously built from the project root without code coverage enabled, the `build_meson.sh` script will automatically reconfigure the build with the coverage option. Similarly, if you later build from the project root using the normal build script, it will automatically disable coverage. Alternatively, you can manually delete the `builddir` directory and run the script again:

```console
   $ rm -rf /path/to/project/root/builddir
   $ ./build_meson.sh  # or ./unittest/build_meson.sh
```

**Using CMake:**

```console
   $ cd unittest
   $ ./build_cmake.sh
```

The resulting binary will be located at `unittest/build/swgenlock_tests`.

**Using Meson:**

```console
   $ cd unittest
   $ ./build_meson.sh
```

The resulting binary will be located at `unittest/builddir/swgenlock_tests`.

**Manual Build (Optional):**

If you prefer to build manually using CMake or Meson commands directly, you must enable code coverage support for the library:

**CMake Manual Build:**
```console
   $ cd /path/to/project/root
   $ mkdir build && cd build
   $ cmake .. -DUNITTEST_ENABLE_COVERAGE=ON
   $ cmake --build . --target vsyncalter_shared
   $ cd ../unittest
   $ mkdir build && cd build
   $ cmake ..
   $ cmake --build .
```

**Meson Manual Build:**
```console
   $ cd /path/to/project/root
   $ meson setup builddir -Denable_coverage=true
   # Or if builddir already exists:
   # $ meson configure builddir -Denable_coverage=true
   $ meson compile -C builddir lib/vsyncalter:shared_library
   $ cd unittest
   $ meson setup builddir
   $ meson compile -C builddir
```

Both automated build scripts and manual builds with the appropriate flags enable coverage instrumentation with `gcov` flags (`-fprofile-arcs -ftest-coverage`) for both the library and unit tests.

PHY Coverage Considerations
---------------------------

The SW GenLock library supports four PHY types:

- **Combo**
- **DKL**
- **C10**
- **C20**

Since a single system may not include support for all PHY types, complete code coverage requires executing the unit tests on multiple platforms. Each platform should support one or more of the target PHY types.

Run Tests and Capture Coverage
------------------------------

**On systems with Combo and DKL PHYs:**

1. Execute the test binary:

**CMake build:**
```console
   $ ./build/swgenlock_tests
```

**Meson build:**
```console
   $ LD_LIBRARY_PATH=../builddir/lib ./builddir/swgenlock_tests
```

For m_n based test execution, add the `--run-mn-test` flag:

```console
   $ ./build/swgenlock_tests --run-mn-test
```

2. Capture the code coverage data:

From within the unittest directory:

**CMake build:**
```console
   $ lcov --capture --directory build --output-file combo_dkl.info
```

**Meson build:**
```console
   $ lcov --capture --directory builddir --output-file combo_dkl.info
**CMake build:**
```console
   $ lcov --capture --directory build --output-file c10.info
```

**Meson build:**
```console
   $ lcov --capture --directory builddir --output-file c1
```console
   $ lcov --capture --directory ../lib/obj --output-file c10.info
```

```console
   $ lcov --capture --directory ../lib/obj --output-file c20.info
```

If the target system is connected to both C10 and C20 monitors, coverage data for both PHY types will be included in a single .info file. This file should be renamed to c10_c20.info for clarity.

Combine Coverage Files
----------------------

Once all `.info` files have been collected from the relevant systems, merge them into a single combined report:

```console
   $ lcov --add-tracefile combo_dkl.info --add-tracefile c10.info --add-tracefile c20.info --output-file final.info
```

This step may be adapted based on which `.info` files are available.


Optional: Remove coverage from system or external libraries:
------------------------------------------------------------

```console
   $ lcov --remove final.info '/usr/*' --ignore-errors unused --output-file final.info
```

Generate HTML Report
--------------------

To produce an HTML report for inspection:

```console
   $ genhtml final.info --output-directory coverage-report
```

The file `coverage-report/index.html` provides a browsable overview of code coverage across the SW GenLock library.

Notes
-----

- Only the **library source code** is included in the final coverage report. Reference applications are excluded.
- Ensure `.gcda` files are not unintentionally overwritten by multiple simultaneous test runs.
- Clean object directories and re-run tests to refresh coverage data where necessary.
