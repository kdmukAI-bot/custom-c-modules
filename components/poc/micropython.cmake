# MicroPython user C module: poc
add_library(usermod_poc INTERFACE)

target_sources(usermod_poc INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src/poc_core.c
    ${CMAKE_CURRENT_LIST_DIR}/modpoc_bindings.c
)

target_include_directories(usermod_poc INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(usermod INTERFACE usermod_poc)
