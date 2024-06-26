Advanced Build Configuration
============================

## Table of Contents
- [Dependencies](#dependencies)
- [Building With CMake](#building-with-cmake)
- [Advanced Building With CMake](#advanced-building-with-cmake)
- [Custom FindUSD.cmake](#The-FindUSD.cmake-helper-script)
- [Setting Up Katana](#setting-up-katana)

## Dependencies
The USD plug-in for Katana has the following dependencies:
- Katana's Plug-ins API
- USD
- TBB
- Python
- Boost
- OpenEXR`**`
- OpenSubdiv`**`

The dependencies need to match the versions that were used to build USD.
The versions we use for these libraries are mentioned in the Katana Dev Guide
https://learn.foundry.com/katana/dev-guide/ExternalSoftware.html

Many of the dependencies are found via CMake's config mechanism.

`** Note:` These dependencies are only needed if using a non standard
pxrConfig.cmake from an external USD Build. Some of these may also not be
needed if they are static dependencies.

Additional information around these dependencies can be found in the
[Advanced Building With CMake](#advanced-building-with-cmake) under
[Advanced Dependencies](#advanced-dependencies).

#### Katana's Plug-ins API
Katana is shipped with a API (header and source files), used to build Katana
plug-ins.
`KATANA_API_LOCATION` must be provided, to the Katana installation prefix.


## Building With CMake

In order to make building the KatanaUsdPlugins as simple as possible we have
included some extra features which can be used for building the plugins if you
have not made any changes to the required libraries. This has been tested
to work with the KatanaUsdPlugins as we ship them. We have provided an example
build script Linux using the `USE_KATANA_THIRDPARTY_LIBS`.

There are further details if you do not wish to use Katana's thirdparty libs
(although it is advised to) in the
[Advanced Dependencies](#advanced-dependencies) section.

### Linux Example
```
cmake .. \
    -DCMAKE_BUILD_TYPE="Release" \
    -DKATANA_API_LOCATION=<KATANA_ROOT> \
    -DUSE_KATANA_THIRDPARTY_LIBS=ON \
    -DCMAKE_INSTALL_PREFIX="/path/to/usd_for_katana/install"
cmake --build . --target install --config Release --parallel 8
```

### Windows Example

```
cmake .. -G Ninja ^
    -DCMAKE_BUILD_TYPE="Release" ^
    -DKATANA_API_LOCATION=<KATANA_ROOT> ^
    -DUSE_KATANA_THIRDPARTY_LIBS=ON ^
    -DHAVE_SNPRINTF=True ^
    -DCMAKE_INSTALL_PREFIX="/path/to/usd_for_katana/install"
cmake --build . --target install --config Release --parallel 8
```

### USE_KATANA_THIRDPARTY_LIBS

For Linux builds only We have introduced a variable which if defined at build
time takes away a lot of the requirements in setting up the thirdparty package
searches within CMake.
This option will define and set the `USE_KATANA_PYTHON`, `USE_KATANA_TBB`,
`USE_KATANA_USD`, and `USE_KATANA_BOOST` variables to `ON`. By default this
is set to `OFF`.

You must specify the `KATANA_API_LOCATION` for this to work, as this is the
root of the Katana install and is used to find the libs and headers we ship
with Katana.

The following options can also be used independently of each other, and you
can default back to using more advanced CMake features for finding these
libraries or using your own by simply not defining any of these.
More details on these methods can be read in the
`cmake/macros/SetupInterfaces.cmake` file.

- `USE_KATANA_PYTHON=<ON/OFF>`
- `USE_KATANA_BOOST=<ON/OFF>`
- `USE_KATANA_TBB=<ON/OFF>`
- `USE_KATANA_USD=<ON/OFF>`

### Optional Libraries and Packages

#### ENABLE_USD_EXPORT
This option allows building the project without UsdExport plugin.
The default value is `ON`. This might be important if building against
Katana version lower than 4.0.

#### ENABLE_USD_RENDER_INFO_PLUGIN
This option allows building the project without UsdRenderInfoPlugin. It
registers the UsdPreviewSurface shaders and UsdLuxLights, and that support for
viewing these in **Viewer (Hydra)** tab was added in Katana 4.0v1. The default
value is `ON`.

## Advanced Building With CMake

Below we provide some examples of cmake build scripts that can be used to
build the plug-ins. There are options there which you may want to remove,
such as `USE_BOOST_NAMESPACE_ENABLED` and `Boost_NAMESPACE` depending
on your build requirements, and whether you're using a namespaced boost,
or want to use our Boost libraries supplied with Katana.

We have introduced a new option, `PXR_PY_PACKAGE_NAME`, to set when building
the Katana USD Plug-ins. For the Foundry USD build, we have namespaced
the Python libraries using this cmake variable, and continued the use of this
cmake variable into our Katana USD Plug-ins. If building against the USD
libraries shipped with Katana, please make sure to set this to `fnpxr`.
If this is not set, you may experience issues using the Python libraries,
most notably for the `usdKatana` module. This value defaults to `pxr`, so if
you are using a default USD build, it will work without setting this value.

### Advanced Dependencies

The following dependencies are required if you are not using the simple build
setup. The majority of dependencies mentioned here will have options depending
heavily on how you have built USD and whether the build used CMake interfaces,
full paths or variables in the pxrConfig.cmake. Below we provide the options
we use.

Note: You may also need more dependencies than those we have mentioned
depending on how you have built USD and whether you have made changes to how
it builds.

If you are using a non standard `pxrConfig.cmake` which uses CMake Targets
instead of the full paths USD implaces by default in `pxrTargets.cmake`, then
you may need to enable `USD_USING_CMAKE_THIRDPARTY_TARGET_DEPENDENCIES=ON` to
allow for finding dependencies on some of these USD libraries. By default this
means `OpenEXR`, and `OpenSubdiv` but other libraries such as `OpenImageIO`
`JPEG`, `PNG`, `TIFF`, `PTex`, may also be needed.

#### USD
`USD_ROOT` must be provided, to the USD installation prefix. USD must be built
with `PXR_ENABLE_PYTHON_SUPPORT` and `PXR_BUILD_IMAGING`.

#### TBB
`TBB_DIR` must be provided, to the cmake folder of the TBB installation.

You may also specify `TBB_tbb_LIBRARY` and `TBB_INCLUDE_HEADERS`, or
`TBB_ROOT_DIR` to specify use of the included findTBB.cmake.

#### Python
`Python_DIR` must be provided, to the folder containing a cmake config file for
the Python installation. In addition, specify:
- `Python_EXECUTABLE` which is used to compile Python files on build

You may also specify the Python_ROOT_DIR to use the default CMake
FindPython.cmake method.

#### Boost
`BOOST_ROOT` must be provided, to the Boost installation prefix. Finding Boost
goes through the standard CMake routes, so set up your `Boost_` variables
appropriately for your Boost build.

#### Advanced Build Linux Example:
```bash
cd /path/to/usd_for_katana
mkdir build
cd build
cmake .. \
    -DKATANA_API_LOCATION=/opt/Foundry/Katana3.2v1/ \
    -DUSD_ROOT=/path/to/USD/ \
    -DPXR_PY_PACKAGE_NAME=fnpxr \
    -DTBB_DIR=/path/to/TBB/cmake \
    -DPython_DIR=/path/to/Python/cmake \
    -DPython_EXECUTABLE=/path/to/Python/bin/python \
    -DBOOST_ROOT=/path/to/Boost \
    -DUSE_BOOST_NAMESPACE_ENABLED=1 \
    -DBoost_NAMESPACE=foundryboost \
    -DBoost_USE_STATIC_LIBS=OFF \
    -DCMAKE_INSTALL_PREFIX=/path/to/usd_for_katana/install

cmake --build . --target install -- -j 18
```

#### Advanced Build Windows Example:
```cmd.exe
cd C:/path/to/usd_for_katana
mkdir build\
cd build
cmake .. -G "Visual Studio 14 2015 Win64"^
    -DCMAKE_BUILD_TYPE="Release"^
    -DKATANA_API_LOCATION="C:/Program Files/Foundry/Katana3.2v1"^
    -DUSD_ROOT="C:/path/to/USD/"^
    -DPXR_PY_PACKAGE_NAME=fnpxr^
    -DTBB_DIR="C:/path/to/TBB/cmake"^
    -DPython_DIR="C:/path/to/Python/cmake"^
    -DPython_EXECUTABLE="C:/path/to/Python/bin/python.exe"^
    -DBOOST_ROOT="C:/path/to/Boost"^
    -DUSE_BOOST_NAMESPACE_ENABLED=1^
    -DBoost_NAMESPACE=foundryboost^
    -DBoost_USE_STATIC_LIBS=OFF^
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

### The FindUSD.cmake helper script

Due to the way the USD pxrConfig.cmake file works, it adds all the libraries
which were built in that USD package. This has the adverse affect of requiring
all the dependent libraries to be included in the build time scripts. To help
in this regard, we have created a simplified findUSD.cmake script to reduce this
to only the libraries required to build the Katana USD Plug-ins.

To enable the use of this FindUSD.cmake file, define the CMake variable
`USE_FOUNDRY_FIND_USD`. This means a considerably shorter build script!
You will also need to specify the `PXR_LIB_PREFIX` you used when building the
USD libraries initially, as this is used to find the libraries. On Linux if
defining this value, remember to add `lib` to the start, for example when
using our USD libraries it would be `libFn`, on Windows just `Fn`.

If you have installed USD in a non-standard way (as we have in Katana), you can
also specify the `USD_INCLUDE_DIR` and `USD_LIBRARY_DIR` separately. Otherwise
it is assumed that these paths are `${USD_ROOT}/include` and `${USD_ROOT}/lib`
respectively. If specifying both of these you do not have to specify `USD_ROOT`.
As we do not support Katana on Apple, the findUSD cmake script
will fail to find libraries on Apple systems.

Below we have included examples of using this for Linux and Windows.


Example Linux CMake:

```
cmake .. \
    -DKATANA_API_LOCATION=<KATANA_ROOT> \
    -DUSD_ROOT=/path/to/USD/ \
    -DPXR_PY_PACKAGE_NAME=fnpxr \
    -DUSD_LIBRARY_DIR=<KATANA_ROOT>/bin \
    -DUSD_INCLUDE_DIR=<KATANA_ROOT>/external/FnUSD/include \
    -DUSE_FOUNDRY_FIND_USD=1 \
    -DPXR_LIB_PREFIX=libFn \
    -DTBB_DIR=/path/to/TBB/cmake \
    -DPython_DIR=/path/to/Python/cmake \
    -DPython_EXECUTABLE=/path/to/Python/bin/python \
    -DBOOST_ROOT=/path/to/Boost \
    -DBoost_NAMESPACE=foundryboost \
    -DUSE_BOOST_NAMESPACE_ENABLED=1 \
    -DBoost_USE_STATIC_LIBS=OFF \
    -DCMAKE_INSTALL_PREFIX="/path/to/usd_for_katana/install"
```

## Setting up for Katana

To enable the USD plug-in for Katana, append the path to the `plugin/`
directory into the `KATANA_RESOURCES` environment variable. For instance, on
#### Linux:

```bash
export KATANA_RESOURCES=$KATANA_RESOURCES:/path/to/usd_for_katana/plugin/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/path/to/usd_for_katana/lib
export PYTHONPATH=$PYTHONPATH:/path/to/usd_for_katana/lib/python
```

#### Windows:

```cmd.exe
set KATANA_RESOURCES=%KATANA_RESOURCES%;C:/path/to/usd_for_katana/plugin/
set PATH=%PATH%;C:/path/to/usd_for_katana/lib
set PYTHONPATH=%PYTHONPATH%;/path/to/usd_for_katana/lib/python
```
