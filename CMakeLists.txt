cmake_minimum_required(VERSION 3.20)
project(ScreamTools VERSION 0.3.0 LANGUAGES C CXX)

option(SCREAM_BUILD_SDK "Build the x86 injected ScreamSDK DLL" ON)
option(SCREAM_BUILD_CLIENT "Build the standalone x86 client fix DLL" ON)
option(SCREAM_BUILD_BUNDLE_TOOL "Build the standalone archive utility" ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(SCREAM_BUILD_BUNDLE_TOOL)
    set(ZLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib")
    add_library(scream_zlib STATIC
        "${ZLIB_ROOT}/adler32.c"
        "${ZLIB_ROOT}/compress.c"
        "${ZLIB_ROOT}/crc32.c"
        "${ZLIB_ROOT}/deflate.c"
        "${ZLIB_ROOT}/infback.c"
        "${ZLIB_ROOT}/inffast.c"
        "${ZLIB_ROOT}/inflate.c"
        "${ZLIB_ROOT}/inftrees.c"
        "${ZLIB_ROOT}/trees.c"
        "${ZLIB_ROOT}/uncompr.c"
        "${ZLIB_ROOT}/zutil.c"
    )
    target_include_directories(scream_zlib PUBLIC "${ZLIB_ROOT}")
    target_compile_definitions(scream_zlib PRIVATE ZLIB_CONST)
    set_target_properties(scream_zlib PROPERTIES POSITION_INDEPENDENT_CODE ON)

    add_subdirectory(ScreamBundleTool)
endif()

if((SCREAM_BUILD_SDK OR SCREAM_BUILD_CLIENT) AND WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "The injected Scream DLLs must be configured with a Win32/x86 toolchain")
    endif()
    # MinHook currently declares compatibility below CMake 3.10. This policy
    # floor keeps new CMake releases quiet without changing its source.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build MinHook statically" FORCE)
    add_subdirectory(third_party/minhook EXCLUDE_FROM_ALL)
endif()

if(SCREAM_BUILD_SDK AND WIN32)
    add_subdirectory(ScreamSDK)
endif()

if(SCREAM_BUILD_CLIENT AND WIN32)
    add_subdirectory(Client)
endif()

if(MSVC AND TARGET ScreamSDK)
    # Opening the generated solution starts on the injected DLL rather than an
    # implementation-only third-party library.
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ScreamSDK)
elseif(MSVC AND TARGET client)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT client)
endif()

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib/LICENSE"
    DESTINATION licenses RENAME zlib-LICENSE.txt)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/LICENSE.txt"
    DESTINATION licenses RENAME imgui-LICENSE.txt)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/minhook/LICENSE.txt"
    DESTINATION licenses RENAME minhook-LICENSE.txt)
