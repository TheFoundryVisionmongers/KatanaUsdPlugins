set(LIB_NAME katanaPluginApi)

set(PUBLICHEADERS
    FnAsset.h
    FnAttributeModifier.h
    FnScenegraphGenerator.h
    FnViewerModifier.h
    FnViewerModifierInput.h
    ScenegraphLocationDelegate.h
)

set(CPPFILES
    ${KATANA_API_SOURCE_DIR}/pystring/pystring.cpp
    ${KATANA_API_SOURCE_DIR}/FnAsset/client/FnDefaultAssetPlugin.cpp
    ${KATANA_API_SOURCE_DIR}/FnAsset/client/FnDefaultFileSequencePlugin.cpp
    ${KATANA_API_SOURCE_DIR}/FnAsset/plugin/FnAsset.cpp
    ${KATANA_API_SOURCE_DIR}/FnAsset/plugin/FnFileSequence.cpp
    ${KATANA_API_SOURCE_DIR}/FnAttribute/client/FnAttribute.cpp
    ${KATANA_API_SOURCE_DIR}/FnAttribute/client/FnAttributeUtils.cpp
    ${KATANA_API_SOURCE_DIR}/FnAttribute/client/FnDataBuilder.cpp
    ${KATANA_API_SOURCE_DIR}/FnAttributeModifier/plugin/FnAttributeModifier.cpp
    ${KATANA_API_SOURCE_DIR}/FnAttributeModifier/plugin/FnAttributeModifierInput.cpp
    ${KATANA_API_SOURCE_DIR}/FnConfig/FnConfig.cpp
    ${KATANA_API_SOURCE_DIR}/FnDefaultAttributeProducer/plugin/FnDefaultAttributeProducerPlugin.cpp
    ${KATANA_API_SOURCE_DIR}/FnDefaultAttributeProducer/plugin/FnDefaultAttributeProducerUtil.cpp
    ${KATANA_API_SOURCE_DIR}/FnLogging/client/FnLogging.cpp
    ${KATANA_API_SOURCE_DIR}/FnPluginSystem/FnPlugin.cpp
    ${KATANA_API_SOURCE_DIR}/FnRenderOutputLocation/client/FnRenderOutputLocationPluginClient.cpp
    ${KATANA_API_SOURCE_DIR}/FnRenderOutputLocation/plugin/FnRenderOutputLocationPlugin.cpp
    ${KATANA_API_SOURCE_DIR}/FnScenegraphGenerator/plugin/FnScenegraphGenerator.cpp
    ${KATANA_API_SOURCE_DIR}/FnScenegraphIterator/client/FnScenegraphIterator.cpp
    ${KATANA_API_SOURCE_DIR}/FnViewerModifier/plugin/FnViewerModifier.cpp
    ${KATANA_API_SOURCE_DIR}/FnViewerModifier/plugin/FnViewerModifierInput.cpp
)

add_library(${LIB_NAME} STATIC ${CPPFILES})
target_include_directories(${LIB_NAME} PUBLIC ${KATANA_API_INCLUDE_DIRS})
set_target_properties(${LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
install(TARGETS ${LIB_NAME} DESTINATION "${INSTALL_SUBDIR}/lib")
install(
    FILES
        ${PUBLICHEADERS}
    DESTINATION
        ${INSTALL_SUBDIR}/include/${LIB_NAME}
)
