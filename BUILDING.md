Advanced Build Configuration
============================

## Table of Contents
- [Dependencies](#dependencies)
- [Building With CMake](#building-with-cmake)
- [Tests](#tests)
- [Setting Up Katana](#setting-up-katana)

## Dependencies
The USD plug-in for Katana has the following dependencies:
- Katana's Plug-ins API
- USD
- TBB
- Python
- Boost
- GLEW

For TBB, Python, Boost and GLEW, the dependencies need to match the
dependencies that were used to build USD.

#### Katana's Plug-ins API
Katana is shipped with a API (header and source files), used to build Katana
plug-ins. A path to such file must be provided in `KATANA_API_LOCATION`.

#### USD
`USD_ROOT` needs to be provided, where header files and library files will be
found. Alternatively, both `USD_INCLUDE_DIR` and `USD_LIBRARY_DIR` can be
provided separetely.

#### TBB
The same TBB version that was used to build USD needs to be provided. There are
several variables that can be set to point to a specific version:
- `TBB_ROOT_DIR`
- `TBB_INCLUDE_DIR`
- `TBB_LIBRARY`
- `TBB_tbb_LIBRARY` (for custom library names)
- `TBB_tbbmalloc_LIBRARY` (for custom library names)

If they are not provided, an attempt to use the system's TBB will be made.

#### Python
The user can specify `PYTHON_INCLUDE_DIR` and `PYTHON_LIBRARY` to use a
specific Python version, which needs to match the version that was used to
build USD. If no specific is provided, the system's Python will be used.

More info in https://cmake.org/cmake/help/latest/module/FindPythonLibs.html.

#### Boost
`BOOST_ROOT` can be provided to point to a specific Boost build, which needs to
match the one that was used for building the USD build. If it is not provided,
an attempt to use the system's Boost will be made.

#### GLEW
`GLEW_INCLUDE_DIR` and `GLEW_LIBRARY` can be provided to point to a specific
GLEW build, that needs to match the version used the USD. If not provided,
an attempt to use the system's version will be made.

## Building With CMake

An example on Linux:

```bash
cd /path/to/usd_for_katana
mkdir build
cd build
cmake .. \
    -DKATANA_API_LOCATION=/opt/Foundry/Katana3.0v7/ \
    -DUSD_ROOT=/path/to/usd/ \
    -DTBB_ROOT_DIR=/path/to/tbb/ \
    -DPYTHON_INCLUDE_DIR=/path/to/python/include/python27/ \
    -DPYTHON_LIBRARY=/path/to/python/lib/libpython2.7.so \
    -DGLEW_INCLUDE_DIR=/path/to/usd/include/ \
    -DGLEW_LIBRARY=/path/to/usd/lib64/libGLEW.so \
    -DBOOST_ROOT=/path/to/boost/ \
    -DCMAKE_INSTALL_PREFIX=/path/to/usd_for_katana/install \
cmake --build . --target install -- -j 18
```

Example on Windows:

```cmd.exe
cd C:/path/to/usd_for_katana
mkdir build\
cd build
cmake ..  -G "Visual Studio 14 2015 Win64"^
    -DCMAKE_BUILD_TYPE="Release"^
    -DKATANA_API_LOCATION="C:/Program Files/Foundry/Katana3.0v7"^
    -DUSD_ROOT="C:/path/to/usd/"^
    -DTBB_ROOT_DIR="C:/path/to/tbb/"^
    -DTBB_tbb_LIBRARY=tbb.lib^
    -DPYTHON_INCLUDE_DIR="C:/Python27/include/"^
    -DPYTHON_LIBRARY="C:/Python27/libs/python27.lib"^
    -DPYTHON_EXECUTABLE="C:/Python27/python.exe"^
    -DGLEW_INCLUDE_DIR="C:/path/to/glew/include/"^
    -DGLEW_LIBRARY="C:/path/to/glew/lib/glew32.lib"^
    -DBOOST_ROOT="C:/path/to/boost/"^
    -DCMAKE_INSTALL_PREFIX="C:/path/to/usd_for_katana/install"

cmake --build . --target install --config Release --parallel 18
```

It is possible to change the installation directory by setting the variable
`CMAKE_INSTALL_PREFIX` before invoking CMake. By default, the plug-in will be
installed in `/usr/local/` on Linux, or in `C:/Program Files/` on Windows,
under the `third_party/katana/` subdirectory. It is encouraged to set the
variable to a more sensible destination other than the default location.

In the provided examples, the plug-in will be effectively installed in
`/path/to/usd_for_katana/third_party/katana/` in Linux, or
`C:/path/to/usd_for_katana/third_party/katana/` in Windows.

## Tests

To optionally build the tests, turn `BUILD_TESTS` on when invoking CMake. After
building, execute the following to run the tests:

```bash
ctest
```

Or, in Windows, if built the optimized configuration:

```cmd.exe
ctest -C Release
```

A valid Katana license will be required to execute the tests.

## Setting Up Katana

To enable the USD plug-in for Katana, append the path to the `plugin/`
directory into the `KATANA_RESOURCES` environment variable. For instance, on
#### Linux:

```bash
export KATANA_RESOURCES=$KATANA_RESOURCES:/path/to/usd_for_katana/third_party/katana/plugin/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/path/to/usd_for_katana/third_party/katana/lib
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/path/to/usd_for_katana/third_party/katana/plugin/Libs
```

#### Windows:

```cmd.exe
set KATANA_RESOURCES=%KATANA_RESOURCES%;C:/path/to/usd_for_katana/third_party/katana/plugin/
set PATH=%PATH%;C:/path/to/usd_for_katana/third_party/katana/lib
set PATH=%PATH%;C:/path/to/usd_for_katana/third_party/katana/plugin/Libs
```

### Dependant Library setup

If USD is not already set up, also adjust the following environment variables:

#### Linux
```bash
export PATH=$PATH:/path/to/usd/bin/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/path/to/usd/lib/
export PYTHONPATH=$PYTHONPATH:/path/to/usd/lib/python/
```

#### Windows:
```cmd.exe
set "PATH=%PATH%;C:/path/to/usd/bin/"
set "PATH=%PATH%;C:/path/to/usd/lib/"
set "PATH=%PATH%;C:/path/to/usd_for_katana/third_party/katana/lib/"
set "PYTHONPATH=%PYTHONPATH%;C:/path/to/usd/lib/python/"
```
You may also need to setup where to find the other dependant libraries Boost,
TBB and GLEW. These can be done in the same way as above, append the lib
directory paths for each dependant library to the PATH or LD_LIBRARY_PATH
dependant on your system. This only applies if using shared libraries.