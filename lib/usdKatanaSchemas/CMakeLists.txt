set(PXR_PACKAGE usdKatanaSchemas)

if(PXR_INSTALL_SUBDIR)
    set(PACKAGE_INSTALL_DIR ${PXR_INSTALL_SUBDIR}/${PXR_PACKAGE})
else()
    set(PACKAGE_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/${PXR_PACKAGE})
endif()

set(PLUG_INFO_ROOT ".")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/plugInfo.json"
    "${PACKAGE_INSTALL_DIR}/plugInfo.json"
)
unset(PLUG_INFO_ROOT)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/generatedSchema.usda"
    "${PACKAGE_INSTALL_DIR}/generatedSchema.usda"
    COPYONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/schema.usda"
    "${PACKAGE_INSTALL_DIR}/${PXR_PACKAGE}/schema.usda"
    COPYONLY
)
