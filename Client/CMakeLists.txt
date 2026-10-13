set(CLIENT_LUA_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/lua/F4Fix.lua"
    "${CMAKE_CURRENT_SOURCE_DIR}/lua/KeybindFix.lua"
    "${CMAKE_CURRENT_SOURCE_DIR}/lua/ReScreamFov.lua"
    "${CMAKE_CURRENT_SOURCE_DIR}/lua/ReScreamPerformance.lua"
    "${CMAKE_CURRENT_SOURCE_DIR}/lua/ReScreamQualityOfLife.lua"
    "${CMAKE_CURRENT_SOURCE_DIR}/lua/LobbyBrowserFix.lua"
    "${CMAKE_CURRENT_SOURCE_DIR}/lua/VersionOverride.lua"
    "${CMAKE_CURRENT_SOURCE_DIR}/lua/StaticBundleProbe.lua"
)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CLIENT_LUA_SOURCES})

function(client_embed_lua source_file output_variable delimiter)
    file(READ "${source_file}" source_text)
    file(READ "${source_file}" source_hex HEX)
    string(TOUPPER "${source_hex}" source_hex)
    if(source_hex MATCHES "^EFBBBF")
        message(FATAL_ERROR "Embedded Lua source must be UTF-8 without BOM: ${source_file}")
    endif()
    string(REGEX MATCHALL ".." source_bytes "${source_hex}")
    foreach(source_byte IN LISTS source_bytes)
        if(source_byte STREQUAL "00")
            message(FATAL_ERROR "Embedded Lua source contains a NUL byte: ${source_file}")
        endif()
    endforeach()
    string(FIND "${source_text}" ")${delimiter}\"" terminator_position)
    if(NOT terminator_position EQUAL -1)
        message(FATAL_ERROR
            "Embedded Lua source contains its C++ raw-string terminator: ${source_file}")
    endif()
    set(${output_variable} "${source_text}" PARENT_SCOPE)
endfunction()

client_embed_lua("${CMAKE_CURRENT_SOURCE_DIR}/lua/F4Fix.lua"
    CLIENT_F4_LUA_SOURCE "RSC_F4")
client_embed_lua("${CMAKE_CURRENT_SOURCE_DIR}/lua/KeybindFix.lua"
    CLIENT_KEYBIND_LUA_SOURCE "RSC_KEYS")
client_embed_lua("${CMAKE_CURRENT_SOURCE_DIR}/lua/ReScreamFov.lua"
    CLIENT_FOV_LUA_SOURCE "RSC_FOV")
client_embed_lua("${CMAKE_CURRENT_SOURCE_DIR}/lua/ReScreamPerformance.lua"
    CLIENT_PERFORMANCE_LUA_SOURCE "RSC_PERF")
client_embed_lua("${CMAKE_CURRENT_SOURCE_DIR}/lua/ReScreamQualityOfLife.lua"
    CLIENT_QOL_LUA_SOURCE "RSC_QOL")
client_embed_lua("${CMAKE_CURRENT_SOURCE_DIR}/lua/LobbyBrowserFix.lua"
    CLIENT_LOBBY_LUA_SOURCE "RSC_LOBBY")
client_embed_lua("${CMAKE_CURRENT_SOURCE_DIR}/lua/VersionOverride.lua"
    CLIENT_VERSION_LUA_SOURCE "RSC_VER")
client_embed_lua("${CMAKE_CURRENT_SOURCE_DIR}/lua/StaticBundleProbe.lua"
    CLIENT_STATIC_PROBE_LUA_SOURCE "RSC_PROBE")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/EmbeddedLuaSources.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/EmbeddedLuaSources.hpp"
    @ONLY NEWLINE_STYLE LF
)

add_library(client SHARED
    src/DllMain.cpp
    src/LuaFixes.cpp
    src/NativeQol.cpp
    src/Log.cpp
    src/Patches.cpp
    ${CLIENT_LUA_SOURCES}
    "${PROJECT_SOURCE_DIR}/ScreamSDK/src/Build.cpp"
)

target_include_directories(client PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    "${PROJECT_SOURCE_DIR}/ScreamSDK/include"
)
set_source_files_properties(${CLIENT_LUA_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
target_compile_definitions(client PRIVATE
    CLIENT_EXPORTS WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS
)
target_link_libraries(client PRIVATE minhook advapi32 user32)
set_target_properties(client PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    OUTPUT_NAME "client"
)

if(MSVC)
    target_compile_options(client PRIVATE /W4 /MP /permissive- /utf-8)
else()
    target_compile_options(client PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS client
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION lib
)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Client/Platform.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Client/Runtime.hpp"
    DESTINATION include/Client
)
