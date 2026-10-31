project(csgo_gc)

add_library(csgo_gc SHARED
    appid.cpp
    case_opening.cpp
    config.cpp
    game_profile.cpp
    gc_client.cpp
    gc_message.cpp
    gc_server.cpp
    gc_shared.cpp
    graffiti.cpp
    inventory.cpp
    item_schema.cpp
    keyvalue.cpp
    main.cpp
    networking_client.cpp
    networking_server.cpp
    rcon_server.cpp
    steam_hook.cpp)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(csgo_gc PRIVATE platform_windows.cpp)
    target_link_libraries(csgo_gc PRIVATE ws2_32)
else()
    target_sources(csgo_gc PRIVATE platform_unix.cpp)
endif()

target_precompile_headers(csgo_gc PRIVATE stdafx.h)

set_target_properties(csgo_gc PROPERTIES PREFIX "")

if (MSVC)
    target_compile_options(csgo_gc PRIVATE /W4 /wd4244)
    target_compile_definitions(csgo_gc PRIVATE _CRT_SECURE_NO_WARNINGS)
    # Emit a linker map so crash dumps (which report only csgo_gc.dll+offset) can be
    # mapped back to function names. Shipped alongside the dll (see OUTDIR copy below).
    target_link_options(csgo_gc PRIVATE /MAP)
else()
    target_compile_options(csgo_gc PRIVATE -Wall -Wextra)
endif()

file(GLOB PROTOBUFS ../protobufs/*.cc)
add_library(csgo_gc_protobufs STATIC ${PROTOBUFS})
set_target_properties(csgo_gc_protobufs PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(csgo_gc_protobufs PUBLIC ../protobufs)
target_link_libraries(csgo_gc_protobufs PUBLIC libprotobuf-lite)
source_group("Protobufs" FILES ${PROTOBUFS})

target_link_libraries(csgo_gc PRIVATE
    cryptopp
    csgo_gc_protobufs
    funchook-static
    steam_api)

# apple message box
if (APPLE)
    target_link_libraries(csgo_gc PRIVATE "-framework Foundation")
endif()

if (OUTDIR)
    add_custom_command(TARGET csgo_gc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:csgo_gc>
        ${OUTDIR}/csgo_gc/${GC_LIB_DIR}/$<TARGET_FILE_NAME:csgo_gc>)

    # Ship the linker map next to the dll for crash symbolization (Windows/MSVC only).
    if (MSVC)
        add_custom_command(TARGET csgo_gc POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE_DIR:csgo_gc>/csgo_gc.map
            ${OUTDIR}/csgo_gc/${GC_LIB_DIR}/csgo_gc.map)
    endif()
endif()

if (BUILD_TESTING)
    add_executable(rcon_server_tests
        config.cpp
        keyvalue.cpp
        rcon_server.cpp
        rcon_server_tests.cpp)

    target_precompile_headers(rcon_server_tests PRIVATE stdafx.h)
    target_link_libraries(rcon_server_tests PRIVATE
        csgo_gc_protobufs
        Threads::Threads)

    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        target_link_libraries(rcon_server_tests PRIVATE ws2_32)
    endif()

    if (MSVC)
        target_compile_options(rcon_server_tests PRIVATE /W4 /wd4244)
        target_compile_definitions(rcon_server_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(rcon_server_tests PRIVATE -Wall -Wextra)
    endif()

    set(RCON_LISTENER_TEST_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/rcon_listener_test_workdir")
    file(MAKE_DIRECTORY "${RCON_LISTENER_TEST_WORKING_DIRECTORY}")
    add_test(NAME rcon_listener_failure COMMAND rcon_server_tests)
    set_tests_properties(rcon_listener_failure PROPERTIES
        WORKING_DIRECTORY "${RCON_LISTENER_TEST_WORKING_DIRECTORY}")

    set(RCON_LOOPBACK_TEST_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/rcon_loopback_test_workdir")
    file(MAKE_DIRECTORY "${RCON_LOOPBACK_TEST_WORKING_DIRECTORY}")
    add_test(NAME rcon_loopback COMMAND rcon_server_tests --loopback)
    set_tests_properties(rcon_loopback PROPERTIES
        WORKING_DIRECTORY "${RCON_LOOPBACK_TEST_WORKING_DIRECTORY}")

    add_executable(inventory_rcon_tests
        case_opening.cpp
        config.cpp
        inventory.cpp
        inventory_rcon_tests.cpp
        item_schema.cpp
        keyvalue.cpp)

    target_precompile_headers(inventory_rcon_tests PRIVATE stdafx.h)
    target_link_libraries(inventory_rcon_tests PRIVATE
        csgo_gc_protobufs
        Threads::Threads)

    if (MSVC)
        target_compile_options(inventory_rcon_tests PRIVATE /W4 /wd4244)
        target_compile_definitions(inventory_rcon_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(inventory_rcon_tests PRIVATE -Wall -Wextra)
    endif()

    # inventory.cpp resolves "../../csgo_gc/inventory.txt" from its CWD.
    set(INVENTORY_RCON_TEST_ROOT "${CMAKE_CURRENT_BINARY_DIR}/inventory_rcon_test_workdir")
    file(MAKE_DIRECTORY "${INVENTORY_RCON_TEST_ROOT}/sub")
    add_test(NAME inventory_rcon_tests COMMAND inventory_rcon_tests)
    set_tests_properties(inventory_rcon_tests PROPERTIES
        WORKING_DIRECTORY "${INVENTORY_RCON_TEST_ROOT}/sub")
endif()
