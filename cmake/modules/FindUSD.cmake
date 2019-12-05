find_library(TBB REQUIRED)
find_package(Python CONFIG REQUIRED)
find_package(Boost COMPONENTS python thread system regex REQUIRED)


#The USD libraries required by the Katana USD Plug-ins
set(USD_LIBRARIES
    gf
    hio
    sdf
    tf
    usdGeom
    usdHydra
    usdImagingGL
    usdLux
    usdRi
    usdShade
    usdSkel
    usdUI
    usdUtils
    vt
)

if(NOT DEFINED USD_LIBRARY_DIR)
    if(NOT DEFINED USD_ROOT)
        message(FATAL_ERROR "Unable to find USD libraries USD_ROOT must be"
            " specified.")
    endif()
    set(USD_LIBRARY_DIR ${USD_ROOT}/lib)
endif()

message("USD_INCLUDE_DIR = ${USD_INCLUDE_DIR}")
if(NOT DEFINED USD_INCLUDE_DIR)
    if(NOT DEFINED USD_ROOT)
        message(FATAL_ERROR "Unable to find USD libraries USD_ROOT must be"
            " specified.")
    endif()
    set(USD_INCLUDE_DIR ${USD_ROOT}/include)
endif()

message("USD_INCLUDE_DIR = ${USD_INCLUDE_DIR}")

set(LIB_EXTENSION "")
if(UNIX AND NOT APPLE)
    set(LIB_EXTENSION .so)
elseif(WIN32)
    set(LIB_EXTENSION .lib)
else()
    message(FATAL_ERROR "Unable to find Apple USD libraries,
        not supported")
endif()

foreach(lib ${USD_LIBRARIES})
    set(USD_${lib}_PATH
        ${USD_LIBRARY_DIR}/${PXR_LIB_PREFIX}${lib}${LIB_EXTENSION})
    if(EXISTS ${USD_${lib}_PATH})
        add_library(${lib} INTERFACE IMPORTED)

        # Probably adding more dependencies than are required to some
        # libraries.
        set(LIBS ${USD_${lib}_PATH}
            Boost::python
            Boost::thread
            Boost::system
            Boost::regex
            TBB::tbb
            Python::Python
            )
        set_target_properties(${lib}
            PROPERTIES
            INTERFACE_LINK_LIBRARIES "${LIBS}"
        )
        set_target_properties(${lib}
            PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${USD_INCLUDE_DIR}"
        )
    else()
        message(STATUS "Unable to add library ${lib}, could not be found in location ${USD_${lib}_PATH}")
    endif()
endforeach()

message("")
