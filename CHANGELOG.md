# Change List

# 19.11_fn6

- TP 462544 - TfToken primvar type was not being handled which results in a warning

## 19.11_fn5

- The evaluateUsdSkelBindings checkbox was added to the parameter interface of UsdIn nodes, adding support for importing bound and rigged models with the UsdSkel schema. When turned on, the skinning will be applied to each prim that is bound to a skeleton.
- New options have been added to disable: UsdExport and UsdRenderInfoPlugin. These are described in the BUILDING.md file.
- TP 458445 - [USD I/O] Fixed narrowing conversion warning/error
- TP 459446 - Added sdr and ndr dependencies and fixed bundling of some Python files.
- TP 458445 - Small fixes to remove standalone compilation warnings.
- TP 455245 - Fixing terminal export and import for shaders.

## 19.11_fn4

- The UsdExport changes are not compatible with Katana releases older than
Katana 4.0v1 due to API changes in `LookFileBakeAPI`. Disable the UsdExport
subdirectory when building for Katana 3. See Github Issue #19:
https://github.com/TheFoundryVisionmongers/KatanaUsdPlugins/issues/19 for more
information on upcoming build changes to support building the latest Katana USD
Plug-ins against older Katana releases.
- The list of renderers that are available inside NetworkMaterialCreate nodes,
which can be accessed by pressing the Shift+Tab keyboard shortcut, adds all
items where their RenderInfoPlugin returns true to
`isNodeTypeSupported("ShadingNode")`. In order to support the display of USD
shading nodes, the list now also includes renderers for which render info
plug-ins, but no render plug-ins are available (viewer-only renderers), if
they meet this condition.
- The Katana USD plug-ins used to always use the default renderer as the
`target` for the shaders. We now use `usd` as the renderer name for shaders
which start with `Usd`, which includes the shading node designed for
UsdPreviewSurface.
- UsdLux light shaders, such as `UsdLuxDiskLight` and `UsdLuxRectLight`, are
now available for use in `GafferThree` nodes.
- ID 447533 - The UsdExport Output Format plug-in, which is designed to work
with UsdMaterialBake nodes, has been hidden from the `outputFormat` parameter
of LookFileBake nodes using the new `Hidden` class variable introduced in
feature enhancement ID 448056.
- ID 447802 - The `USD_KATANA_ALLOW_CUSTOM_MATERIAL_SCOPES` environment
variable of the Katana USD Plug-ins is now set to true by default. If set to
false, this will limit material assignments to materials scoped under a Looks
location.
- ID 447804 - The `USD_KATANA_ADD_CUSTOM_PROPERTIES` environment variable of the
Katana USD Plug-ins is now set to true by default. If set to false, this will
silently ignore custom properties.
- GitHub Issue #6 - Pull Request #7 - Avoid flattening face-varying primvars
during import.

## v19.11_fn3

- TP 420782 - Fixing issues when building inside the Katana build pipeline
- TP 427390(Issue #3) - Fixed incorrect lib prefix during USD linking using the
USE_KATANA_THIRDPARTY_LIBS option

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
