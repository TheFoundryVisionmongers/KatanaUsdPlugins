# Change List

# 21.05_fn7_py2

## Bug fixes
- ID-536039 - USD Shader Prims with namespaced `shaderId` attribute values were not converted to the correct shader ID for Katana Attributes.
- ID-539855 - Loading the same USD Layer with UsdIn could result in instance prototype location paths changing in their order randomly.
- ID-534163 - When importing a USD file using the UsdIn node with the isolatePath parameter, if the file contained a light list cache with any lights which weren't descendents of the path specified in isolatePath, then an error would be shown in the scenegraph.

# 21.05_fn6_py2 - 21.05_fn6_py3

- ID-514557 - Substituted `GetLocationStack()` with the more performant `GetLocationStackStringView()`. Requires 4.5v4 or 5.0v4 onwards.
- ID 514589 - Bezier curves should create a vstep attribute when bringing them in UsdIn
- ID 514440 - Add support of widths with different interpolation scope for curves imported with UsdIn
- ID 513613 - Add support of wrap type for basis curves
- ID 505169 - Align USD interpolation scope with Prman formula used in GeoLib3
- ID 508782 - Rename PxrVt to Vt to remove Pxr prefix
- ID 506788 - Added bxdf to SdfType conversion mapping & use Token by default
- ID 509475 - Removing legacy logic to deal with osl prman shaders

# 21.05_fn5_py2 - 21.05_fn5_py3

## Bug fixes
- ID 501927 - Fixed a variety of build issues when building KatanaUsdPlugins externally
    - KatanaUsdPlugins built externally would not build against Katana 5.0 due to not finding the correct Boost Python package.
    - KatanaUsdPlugins built externally installed the UsdExport modules to the wrong location.
    - KatanaUsdPlugins default PXR_LIB_PREFIX was set incorrectly when using Katanas USD at build time.
    - KatanaUsdPlugins did not specify the `CMAKE_CXX_STANDARD`.
    - KatanaUsdPlugins did not always find pyconfig.h correctly.
    - KatanaUsdPlugins could not find the Linux TBB library when using Katana's TBB libraries.
    - KatanaUsdPlugins did not define Katana's `PXR_PY_PACKAGE_NAME` correctly by default when using Katana's USD Libraries.

# 21.05_fn4_py2 - 21.05_fn4_py3

## Bug fixes
- ID 508168 - Set **BinPackArguments** to `false` in the Clang-Format file (KatanaUsdPlugins)

- ID 453348 - When importing a USD file that had previously been exported from Katana via the UsdMaterialBake node, any Katana child materials were not bound correctly on import.  For example, the materialAssign attribute on a bound location was updated from NetworkMaterial1_material1 to NetworkMaterial1/material1 (to match what it was before it was exported), but the location of the material in the scene graph was still NetworkMaterial1_material1. This meant the location was unbound and produced incorrect rendering results.

 - ID 505055 - Ops in KatanaUsdPlugins clashed with RenderMan named ops. Pxr has been removed as a prefix to the Ops registered by KatanaUsdPlugins. The folders of KatanaUsdPlugins source code have been renamed to remove Pxr prefixes where applicable.

# 21.05_fn3_py2 - 21.05_fn3_py3

## Bug fixes
- ID 499422 -
    - Convert USD shader parameter name to the implementation name when imported into Katana and vice-versa when exported or passed on to Hydra.
    - Use the `sdrUsdDefinitionType` shader input hint to define what type to give parameters passed through Hydra.
    - Convert shader input values which have been designated as Asset Identifiers to anSdfAssetPath when passed through Hydra.
- ID 491565 - UsdIn will add imported prims with the 'Light' prim type to the `lightList` attribute on '/root/world'. UsdMaterialBake will write lights with the 'Light' prim type to the USD 'lightList' cache. Any exported USD `lightList` cache will use the `consumeAndHalt` cache mode by default.
- ID 499328 - Instancing prototype locations which exist as direct children of the instancer are correctly given the `instance source` type.
MISC - Libraries created with the pxr_library function compile with C++14

## Feature Enhancements

- ID 499854 - Motion Samples are now read for UsdSkel assets when importing with UsdIn if available and the motionSampleTimes parameter is set with multiple fallback sample points.

# 21.05_fn2_py2 - 21.05_fn2_py3

## Bug fixes
- ID 490802 - Lights imported through UsdIn have the lightList attribute 'enabled' set to `true` by default.
- ID 446730 - When trying to overwrite variants already baked from a UsdMaterialBake node, an error can be printed to the terminal, resulting in the USD file not being written. In this instance, flushing caches before overwriting the file should act as a workaround.
- ID 488546 - Mute and Solo of lights imported from USD without `UsdLuxLightListAPI` information, and adopted for editing by a gafferThree node.

## Feature Enhancements
- ID 490830 - Environment variable `USD_IMPORT_USD_LUX_LIGHTS_WITH_PRMAN_SHADERS` added to KatanaUsdPlugins to determine whether base UsdLux lights should import with RenderMan light attributes, in addition to USD attributes, if the RenderMan renderer is detected. By default this behaviour is off.
- ID 447830 - Texture paths linking to relative UDIM paths will now resolved relative to the USD layer which contains the attribute value.

# 21.05_fn1_py3

- Updated codebase with changes for Python3 compliance.

# 21.05_fn1_py2

## Bug fixes
- ID 485105 - UsdMaterialBake node would write out childMaterial's shaderInput with a double up of the connection type  (for example`float inputs:inputs:roughness = 0.4` rather than `float inputs:roughness = 0.4`.)
- ID 487750 - Some UsdLux light attributes were not imported with the matching name as Katana attributes.
- ID 485192 - When using a UsdIn node to import a USD file containing a 3-dimensional texture coordinate primvar named **\<primname>**, the attributes under **geometry.arbitrary.\<primname>** were not loaded completely.
- ID 483676 - When baking a material network via a **UsdMaterialBake** node, expansion states would not be written correctly.

## Feature Enhancements
- ID 489436 - UsdExport will now export light and shadow linking information.
Resolved light linking paths will be written to USD, so long as the location linked to, and the light are under the same rootLocation specified on **UsdMaterialBake**
- ID 484656 - Lights using the LightAPI schema with `katana:id` attributes will be imported as the provided shader type, with all the renderer name-spaced attributes being loaded into that renderers light parameters.  If multiple shaders are mentioned in the `katana:id` attribute, multiple shaders will be imported, and this is why the name-spaced attributes are important.  You may not have multiple shaders for the same renderer.  We utilise the `SdrRegistry` to discover the light shader and to read in the relevant attributes, and understand the context required; this means it is required that the light shader is registered to the `SdrRegistry` for the USD library KatanaUsdPlugins is utilising.

  UsdLux Light information will be imported as before using the UsdLuxAPI.

  The logic to import UsdLux information into `prmanLightShader` and `prmanLightParameters` has been moved to a location decorator, and will be skipped if the `prman` renderer isn't present in the environment, or if the `prmanLightShader` has been populated by the `SdrRegistry` based logic mentioned above.
- ID 484483 - The `PxrUsdIn` Op no longer reads and writes camera or light lists to the location provided by a UsdIn node's location parameter.

  A new Op `PxrUsdInUpdateGlobalListsOp`, which runs after UsdIn's current Op(s), has been added to set **globals.cameraList** and lightList directly on the **/root/world** location; provided they are within a model hierarchy with the USD Stage.
- ID 487862 - A USD Export plugin has been added to write light location attributes based on parameters from the Sdr Registry. Where a node exists for a light shader, attribute names which match an input identifier are written with the input's type information. This process occurs after light locations are written via the UsdLux API and any parameters written through that are preserved.
- ID 487878 - UsdExport will now parse the exported USD file for any exported lights using the UsdLuxListAPI and write this to the Root Prim, for each variant.
- ID 487875 - A LightAPI schema is applied to lights exported through the **UsdMaterialBake** node containing Katana-specific parameters.
- ID 487630 - A new API `UsdExportPlugins` has been added to allow further customization of exported data to USD without having to edit and rebuild KatanaUsdPlugins.  More information on this can be found in the Developer Guide **Katana USD Plug-ins > UsdExport > UsdExport Plug-ins**.
- ID 471650 - Lights imported via a UsdIn node are adopted by right clicking their entry in a GafferThree node and selecting **Adopt for Editing** where **Show Incoming** Scene is enabled.
- ID 487661 - Exported default Prims will now have a Kind assigned to them:
  - Assembly file default Prim will be of Kind assembly.
  - Shading file default Prim will be of Kind component.
- ID 484359 - The schema classes that are part of the Katana USD Plug-ins have been rebuilt using the usdGenSchema code generator script that ships with USD 21.05.
- ID 484299 - `usdImagingGL` has been replaced by `usdImaging` as a dependency for the `usdKatana` library in `KatanaUsdPlugins`.
- ID 472106 - Added UsdTransform2d nodes to usd shaders.

# 19.11_fn9

## Bug Fixes
- ID 486520 - Removed paths to _Resources/_ from .arclint
- ID 482198 - When using the **isolatePath** parameter on a UsdIn node that is loading a USD asset that uses light lists, UsdIn failed with an error message: _UsdIn: Failed to compute katana path for usd path_
- ID 479726 - When setting the **isolatePath** parameter on a UsdIn node, and then trying to use a UsdInIsolate node, UsdIn failed with an error message: UsdIn: Failed to compute katana path for usd path

  The value of the **isolatePath** parameter is now prefixed onto the USD stage mask path set up by UsdInIsolate.

  When using **isolatePath** and UsdInIsolate in the same node graph, the mask created by UsdInIsolate (using Katana scene graph locations) did not take account of the UsdIn mask path which did not contain the locations cut out by setting UsdIn's **isolatePath**. This meant the USD stage was masked by locations which did not exist, causing errors.
- ID 479729 - When using a UsdInIsolate node to isolate a location that did not exist, Katana crashed.
- ID 480335 - Updating clang-format to 100 column line length
- ID 471351 - When attempting to bake a Network Material containing custom OSL shaders to USD files using a UsdMaterialBake node, a Python exception was raised, due to an apparent `TfToken` type mismatch. A resulting USD file then contained shader inputs appearing with a wrong type and default value, for example `color3f inputs:MyFloat = (0, 0, 0)` instead of `float inputs:MyFloat = 0.58`.

  Shader inputs not found in the `SdrRegistry` but with valid shader output tags may now assume their respective USD data type. Note that this is an estimation, and is not guaranteed to always be correct, if the input attributes are not found in the USD `SdrRegistry`.
- ID 470559 - When attempting to bake a Network Material containing a shading node whose name contained spaces to USD files using a **UsdMaterialBake** node, a Python exception was raised.
- ID 467244 - When applied to a material network containing shading nodes connected via the per-component ports (e.g. `color.r`), the **UsdMaterialBake** node would fail to write out shader connections properly.  The delimiting dots in these names are now converted to colons in the USD file (e.g. `color:r`) to produce valid USD attribute names.

# 19.11_fn8

## Feature Enhancements
- ID 462957 - Integrated updates made to KatanaUsdPlugins by Pixar in USD 20.02:Fixed incorrect geometry.instanceSource attribute for nested instances. (PR#1015)
    - VtKatanaMapOrCopy(FnAttribute) now returns a map of all motion samples.
    - Removed support for deprecated USD format usdb.
- ID 463829 - Issue #21: Support for cylinder lights (UsdLuxCylinderLight) has been added to UsdIn.
- ID 463832 - Issue #18: Nested instances can now be imported via the as sources and instances option of the instanceMode parameter of a UsdIn node

## Bug Fixes

 - ID 445637 - Issue #13: When importing point-based prims via UsdIn, the accelerations attribute was not read. (PR #14)
 - ID 465844 - When baking out materials containing PxrLayer nodes using UsdMaterialBake, an error was raised and the material would not be baked correctly.
 - ID 466070 - When attempting to export a network material containing a ShadingNodeArrayConnector node using UsdMaterialBake, an exception was raised. (Currently, ShadingNodeArrayConnector nodes are not supported by UsdMaterialBake.)
 - ID 466547 - Registering multiple shaders in the USD SdrRegistry with the same name caused issues when finding default parameters for the Hydra Viewer and the usd renderer info plug-in.
 - ID 470257 - When attempting to export a material without layout attributes using UsdMaterialBake, an exception was raised.
 - ID 470839 - Adding `/bigobj` flag to Windows Debug build
 - ID 430007 - Wrap attribute values in `str()` when mixed with `DelimiterEncode()` for Python 3 compatibility.

# 20.08_fn1

- TP 462395 - Update KatanaUsdPlugins to use 20.08 compatible API

# 19.11_fn7

## Feature Enhancements
- UsdMaterialBake nodes now also export **prmanStatements.attributes** set on a location.
- The following changes have been made in the parameter interface of UsdMaterialBake nodes:
    - The **fileName** parameter was renamed to **looksFilename** (note the lower-case `n`).
    - The **fileFormat** parameter was renamed to **looksFileFormat**.
    - The **createCompleteUsdAssemblyFile** parameter was added, which controls whether a USD assembly file is written alongside the baked material looks file. The USD assembly file references the looks file (**looksFilename**) and a particular payload file (see **payloadFilename** below). Defaults to No. When set to Yes, the following additional parameters appear:
        - **assemblyFilename** -- the name of the assembly file to write.
        - **payloadFilename** -- the name of a file to reference as a payload in the assembly file to write.

    **Warning**: The changes in parameter inferface of UsdMaterailBake nodes are not compatible with earlier releases, meaning that attemping to open a Katana project with the new nodes with an earlier plugin results in Python exceptions being raised.
- UsdIn nodes will now import **material.layout** attributes, allowing for the shading nodes to be edited inside of NetworkMaterialEdit nodes, retaining any positional, color or viewstate information relating to USD's `UsdUINodeGraphNodeAPI`.
- Boost::filesystem added as a dependency onto usdKatana library for bugfix ID 462435.

## Bug Fixes
- ID 440214 - Issue #11: (Windows only) `.lib` files were not installed along with `.dll` files for the **usdKatana** and **vtKatana** libraries in `plugins\Resources\Usd\lib\` and the shipped plug-ins in `plugins\Resources\Usd\plugin\Libs\`.
- ID 440218 - Issue #9: When installing the Katana USD Plug-ins, public class headers were not installed correctly in `plugins/Resources/Usd/include/<library name>/`.
- ID 446328 - When exporting and then importing a material via UsdMaterialBake and UsdIn nodes, the grouping of parameters in pages was not restored correctly.
- ID 453346 - When exporting material information using a UsdMaterialBake node, child material locations with no local change were not exported.
- ID 462435 - When using UsdIn nodes to load USD assets that use textures containing `<UDIM>` tokens in their filenames, spurious warning messages about unresolved asset paths were logged.
- ID 462544 - When loading a USD asset that made use of a `TfToken` primvar into the Hydra Viewer, a warning message was logged, and the primvar was ignored.
- ID 463889 - When creating shading nodes from USD, a wrong target renderer was set in certain cases.

# 19.11_fn6

- TP 459692 - [USD I/O] Supporting Ramp widgets with UsdExport and Import
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
    :kat:node:`UsdIn`. See `Table of Name Changes`_.
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
