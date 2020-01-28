# Change List

## v19.11_fn2

- Changed variable requirement from `PYTHON_EXECUTABLE` to `Python_EXECUTABLE`
to be inline with the `findPython` provided by CMake.
- Changed the build process to make the use of CMake config files as optional.
- Added a new set of options for building which simplify the build_script to
use the libraries and headers shipped with Katana, ensuring compatibility with
our USD. See the new `USE_KATANA_THIRDPARTY_LIBS` section in the `Building.md`
- Updated `Building.md` to document new build options.
- Added new `SetupInterfaces.cmake` to cover new build interface options.
- Fixed issue where headers defined under the PUBLIC_HEADERS argument were not
installed into an include folder in the install folder.

## v19.11_fn1

- Removed the :kat:node:`Pxr` prefix from the node type names to make them
    match Katana naming conventions, eg. :kat:node:`UsdIn` instead of
    :kat:node:`PxrUsdIn`. See `Table of Name Changes`_.
- Changed the name of the Op types to ensure we don't clash with
    externally-built Katana USD Plug-ins.
- Updated Attribute names to more generic names.
- Removed automatic creation of `prmanStatements`.  Only added if `prman` is a
    loaded renderer.
- Added a `Support.cmake` file to support the Pixar macro usage in build
    scripts.
- Created a root `CMakeLists.txt` in order to replace the USD core
    `CMakeLists.txt`.
- Ensured the CMake builds work on Windows and Linux with Katana's
    `fnUsd` libraries.
- Using CMake configurations over absolute library paths for portability.
- Added `Bootstrap` to `usdKatana` libraries to set up Katana plug-ins.
- Changed use of `std::regex` to `boost::regex` due to issues with GCC 4.8.x
    and C++11 `std::regex`.
- Patched issues with builds on Visual Studio 15.
- Modified `vtKatana` library to export `long` data type.
- Removed deprecated code support for RenderMan coshaders.
- Removed deprecated code for the USD `VMP`.
- Removed header files which were no longer used in `KatanaPluginApi`.
- Added support for flushing of caches to UsdIn Op type (was previously taken
    care of by removed USD VMP - see f)).
- Updated Apache 2.0 licenses and added `NOTICE.txt`.
- Updated `README.md` and added `BUILDING.md`.
- Updated the default option for `USD_ABC_WRITE_UV_AS_ST_TEXCOORD2FARRAY` to be
    "true" from "false", to ensure the USD Alembic plug-in imports uv
    attributes into the st arbitrary attribute location; matching the behavior
    of :kat:node:`Alembic_In` nodes.
- Our plug-in uses `fnpxr` python modules.

## v19.11 (Initial)

- Initial code started from 19.11 USD extracting the third_party/katana
    folder.
https://github.com/PixarAnimationStudios/USD/blob/master/CHANGELOG.md
