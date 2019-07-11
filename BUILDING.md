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
- OpenEXR
- OpenImageIO
- JPEG
- PNG
- TIFF
- Zlib
- OpenSubdiv
- PTex

The dependencies need to match the versions that were used to build USD.

Many of the dependencies are found via CMake's config mechanism.

#### Katana's Plug-ins API
Katana is shipped with a API (header and source files), used to build Katana
plug-ins.
`KATANA_API_LOCATION` must be provided, to the Katana installation prefix.

#### USD
`USD_ROOT` must be provided, to the USD installation prefix.

#### TBB
`TBB_DIR` must be provided, to the cmake folder of the TBB installation.

#### Python
`Python_DIR` must be provided, to the cmake folder of the Python installation.
In addition, specify:
- `PYTHON_EXECUTABLE`
to use to compile Python code.

#### Boost
`BOOST_ROOT` must be provided, to the Boost installation prefix.
Dynamic Boost is used, so specifying
- `Boost_USE_STATIC_LIBS=OFF`
is also useful.

#### GLEW
`GLEW_DIR` must be provided, to the cmake folder of the GLEW installation.

#### OpenEXR
`OpenEXR_DIR` must be provided, to the cmake folder of the OpenEXR installation.

#### OpenImageIO
`OpenImageIO_DIR` must be provided, to the cmake folder of the OpenImageIO installation.

#### JPEG
The following must be provided:
- `JPEG_INCLUDE_DIR` to the location of JPEG headers
- `JPEG_LIBRARY_RELEASE` to the location of the JPEG library to link

#### PNG
The following must be provided:
- `PNG_PNG_INCLUDE_DIR` to the location of PNG headers
- `PNG_LIBRARY` to the location of the PNG library to link

#### TIFF
The following must be provided:
- `TIFF_INCLUDE_DIR` to the location of TIFF headers
- `TIFF_LIBRARY` to the location of the TIFF library to link

#### Zlib
`ZLIB_ROOT` must be provided, to the Zlib installation prefix.

#### OpenSubdiv
`OpenSubdiv_DIR` must be provided, to the cmake folder of the OpenSubdiv installation.

#### PTex
`PTex_DIR` must be provided, to the cmake folder of the PTex installation.

## Building With CMake

An example on Linux:

```bash
cd /path/to/usd_for_katana
mkdir build
cd build
cmake .. \
    -DKATANA_API_LOCATION=/opt/Foundry/Katana3.2v1/ \
    -DUSD_ROOT=/path/to/USD/^
    -DTBB_DIR=/path/to/TBB/cmake^
    -DPython_DIR=/path/to/Python/cmake^
    -DPYTHON_EXECUTABLE=/path/to/Python/bin/python.exe^
    -DBOOST_ROOT=/path/to/Boost^
    -DBoost_USE_STATIC_LIBS=OFF^
    -DGLEW_DIR=/path/to/GLEW/lib/cmake/glew^
    -DOpenEXR_DIR=/path/to/OpenEXR/cmake^
    -DOpenImageIO_DIR=/path/to/OpenImageIO/cmake^
    -DJPEG_INCLUDE_DIR=/path/to/JPEG/include^
    -DJPEG_LIBRARY_RELEASE=/path/to/JPEG/lib/libjpeg.lib^
    -DPNG_PNG_INCLUDE_DIR=/path/to/PNG/include^
    -DPNG_LIBRARY=/path/to/JPEG/lib/libpng.lib^
    -DTIFF_INCLUDE_DIR=/path/to/TIFF/include^
    -DTIFF_LIBRARY=/path/to/JPEG/lib/libtiff.lib^
    -DZLIB_ROOT=/path/to/Zlib^
    -DOpenSubdiv_DIR=/path/to/OpenSubdiv/cmake^
    -DPTex_DIR=/path/to/PTex/cmake^
    -DCMAKE_INSTALL_PREFIX=/path/to/usd_for_katana/install

cmake --build . --target install -- -j 18
```

Example on Windows:

```cmd.exe
cd C:/path/to/usd_for_katana
mkdir build\
cd build
cmake ..  -G "Visual Studio 14 2015 Win64"^
    -DCMAKE_BUILD_TYPE="Release"^
    -DKATANA_API_LOCATION="C:/Program Files/Foundry/Katana3.2v1"^
    -DUSD_ROOT="C:/path/to/USD/"^
    -DTBB_DIR="C:/path/to/TBB/cmake"^
    -DPython_DIR="C:/path/to/Python/cmake"^
    -DPYTHON_EXECUTABLE="C:/path/to/Python/bin/python.exe"^
    -DBOOST_ROOT="C:/path/to/Boost"^
    -DBoost_USE_STATIC_LIBS=OFF^
    -DGLEW_DIR="C:/path/to/GLEW/lib/cmake/glew"^
    -DOpenEXR_DIR="C:/path/to/OpenEXR/cmake"^
    -DOpenImageIO_DIR="C:/path/to/OpenImageIO/cmake"^
    -DJPEG_INCLUDE_DIR="C:/path/to/JPEG/include"^
    -DJPEG_LIBRARY_RELEASE="C:/path/to/JPEG/lib/libjpeg.lib"^
    -DPNG_PNG_INCLUDE_DIR="C:/path/to/PNG/include"^
    -DPNG_LIBRARY="C:/path/to/JPEG/lib/libpng.lib"^
    -DTIFF_INCLUDE_DIR="C:/path/to/TIFF/include"^
    -DTIFF_LIBRARY="C:/path/to/JPEG/lib/libtiff.lib"^
    -DZLIB_ROOT="C:/path/to/Zlib"^
    -DOpenSubdiv_DIR="C:/path/to/OpenSubdiv/cmake"^
    -DPTex_DIR="C:/path/to/PTex/cmake"^
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
