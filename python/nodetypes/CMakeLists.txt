set(NODE_TYPES
    PxrUsdIn
)

set(OUTPUT_DIR ${INSTALL_SUBDIR}/plugin/Plugins/NodeTypes)

set(PYFILES "")
set(IMPORT_LINES "")

foreach(nodeType ${NODE_TYPES})
    list(APPEND PYFILES ${nodeType}.py)
    set(IMPORT_LINES "${IMPORT_LINES}import ${nodeType}\n")
endforeach()

install(
    PROGRAMS ${PYFILES}
    DESTINATION ${OUTPUT_DIR}
)

# Install a __init__.py that imports all the known node types
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated_NodeTypes_init.py"
        "${IMPORT_LINES}")
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/generated_NodeTypes_init.py"
    DESTINATION "${OUTPUT_DIR}"
    RENAME "__init__.py"
)
