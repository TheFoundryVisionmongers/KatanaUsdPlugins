# Copyright 2019 Foundry
#
# Licensed under the Apache License, Version 2.0 (the "Apache License")
# with the following modification; you may not use this file except in
# compliance with the Apache License and the following modification to it:
# Section 6. Trademarks. is deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the trade
# names, trademarks, service marks, or product names of the Licensor
# and its affiliates, except as required to comply with Section 4(c) of
# the License and to reproduce the content of the NOTICE file.
#
# You may obtain a copy of the Apache License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the Apache License with the above modification is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the Apache License for the specific
# language governing permissions and limitations under the Apache License.


#The USD libraries required by the Katana USD Plug-ins
set(USD_LIBRARIES
    ar
    arch
    boost
    cameraUtil
    geomUtil
    gf
    hd
    hdar
    hf
    hio
    js
    kind
    pcp
    pegtl
    plug
    pxOsd
    python
    sdf
    sdr
    tf
    ts
    trace
    usd
    usdGeom
    usdHydra
    usdImaging
    usdLux
    usdRender
    usdRi
    usdShade
    usdSkel
    usdUI
    usdUtils
    usdVol
    vt
    work
)

# Auto generated from pxrTargets.cmake from our build using the script below:
# ------------------------------------------------------------------------------
# Provide USD_ROOT as a defenition to where the USD you want to
# print out dependnecies for is available.
# include(${USD_ROOT}/cmake/pxrTargets.cmake)
# set(USD_LIBS
#    ...
# )
# foreach(usd_lib ${USD_LIBS})
#     get_target_property(dep_list ${usd_lib} INTERFACE_LINK_LIBRARIES)
#     message("set(USD_${usd_lib}_DEPENDENCIES ${dep_list})")
# endforeach()
# ------------------------------------------------------------------------------

set(USD_ar_DEPENDENCIES arch;js;tf;plug;vt;python;TBB::tbb;Python3::Python)
set(USD_arch_DEPENDENCIES )
set(USD_boost_DEPENDENCIES )
set(USD_cameraUtil_DEPENDENCIES tf;gf)
set(USD_gf_DEPENDENCIES arch;tf;python;Python3::Python)
set(USD_geomUtil_DEPENDENCIES arch;gf;tf;vt;pxOsd)
set(USD_hd_DEPENDENCIES plug;tf;trace;vt;work;sdf;cameraUtil;hf;pxOsd;sdr;TBB::tbb)
set(USD_hdar_DEPENDENCIES hd;ar)
set(USD_hf_DEPENDENCIES plug;tf;trace)
set(USD_hio_DEPENDENCIES arch;js;plug;tf;vt;trace;ar;hf)
set(USD_js_DEPENDENCIES tf)
set(USD_kind_DEPENDENCIES tf;plug)
set(USD_pcp_DEPENDENCIES tf;trace;vt;sdf;work;ar;python;TBB::tbb;Python3::Python)
set(USD_pegtl_DEPENDENCIES arch)
set(USD_plug_DEPENDENCIES arch;tf;js;trace;work;TBB::tbb)
set(USD_pxOsd_DEPENDENCIES tf;gf;vt)
set(USD_python_DEPENDENCIES boost;Python3::Python)
set(USD_sdf_DEPENDENCIES arch;tf;gf;pegtl;trace;ts;vt;work;ar;python;TBB::tbb;Python3::Python)
set(USD_sdr_DEPENDENCIES arch;plug;trace;tf;vt;work;ar;sdf)
set(USD_tf_DEPENDENCIES arch;python;TBB::tbb;Python3::Python)
set(USD_ts_DEPENDENCIES vt;gf;tf)
set(USD_trace_DEPENDENCIES arch;js;tf;TBB::tbb)
set(USD_usd_DEPENDENCIES arch;kind;pcp;sdf;ar;plug;tf;trace;ts;vt;work;python;TBB::tbb;Python3::Python)
set(USD_usdGeom_DEPENDENCIES js;tf;plug;vt;sdf;trace;usd;work;TBB::tbb)
set(USD_usdHydra_DEPENDENCIES tf;usd;usdShade)
set(USD_usdImaging_DEPENDENCIES gf;tf;plug;trace;vt;work;geomUtil;hd;hdar;hio;pxOsd;sdf;usd;usdGeom;usdLux;usdRender;usdShade;usdSkel;usdVol;ar;TBB::tbb)
set(USD_usdLux_DEPENDENCIES tf;vt;sdf;usd;usdGeom;usdShade)
set(USD_usdRender_DEPENDENCIES gf;tf;usd;usdGeom;usdShade)
set(USD_usdRi_DEPENDENCIES tf;vt;sdf;usd;usdShade;usdGeom)
set(USD_usdShade_DEPENDENCIES tf;vt;js;sdf;sdr;usd;usdGeom;TBB::tbb)
set(USD_usdSkel_DEPENDENCIES arch;gf;tf;trace;vt;work;sdf;usd;usdGeom;TBB::tbb)
set(USD_usdUI_DEPENDENCIES tf;vt;sdf;usd)
set(USD_usdUtils_DEPENDENCIES arch;tf;gf;sdf;usd;usdGeom;usdShade;usdUI;TBB::tbb)
set(USD_usdVol_DEPENDENCIES tf;usd;usdGeom)
set(USD_vt_DEPENDENCIES arch;tf;gf;trace;python;TBB::tbb;Python3::Python)
set(USD_work_DEPENDENCIES tf;trace;TBB::tbb)

if(NOT DEFINED USD_LIBRARY_DIR)
    if(NOT DEFINED USD_ROOT)
        message(FATAL_ERROR "Unable to find USD libraries USD_ROOT must be"
            " specified.")
    endif()
    set(USD_LIBRARY_DIR ${USD_ROOT}/lib)
endif()

if(NOT DEFINED USD_INCLUDE_DIR)
    if(NOT DEFINED USD_ROOT)
        message(FATAL_ERROR "Unable to find USD libraries USD_ROOT must be"
            " specified.")
    endif()
    set(USD_INCLUDE_DIR ${USD_ROOT}/include)
endif()


set(LIB_EXTENSION "")
if(UNIX AND NOT APPLE)
    set(LIB_EXTENSION .so)
elseif(WIN32)
    set(LIB_EXTENSION .lib)
endif()

foreach(lib ${USD_LIBRARIES})
    set(USD_${lib}_PATH
        ${USD_LIBRARY_DIR}/${PXR_LIB_PREFIX}${lib}${LIB_EXTENSION})
    if(EXISTS ${USD_${lib}_PATH})
        add_library(${lib} INTERFACE IMPORTED)

        set_target_properties(${lib}
            PROPERTIES
            INTERFACE_LINK_LIBRARIES "${USD_${lib}_PATH};${USD_${lib}_DEPENDENCIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${USD_INCLUDE_DIR}"
        )
    else()
        message(FATAL_ERROR "Unable to add library ${lib}, "
            "could not be found in location ${USD_${lib}_PATH}")
    endif()
endforeach()
