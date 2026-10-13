set(IMGUI_ROOT "${PROJECT_SOURCE_DIR}/third_party/imgui")

add_library(scream_imgui STATIC
    "${IMGUI_ROOT}/imgui.cpp"
    "${IMGUI_ROOT}/imgui_draw.cpp"
    "${IMGUI_ROOT}/imgui_tables.cpp"
    "${IMGUI_ROOT}/imgui_widgets.cpp"
    "${IMGUI_ROOT}/backends/imgui_impl_win32.cpp"
    "${IMGUI_ROOT}/backends/imgui_impl_dx9.cpp"
)
target_include_directories(scream_imgui PUBLIC "${IMGUI_ROOT}")
target_compile_definitions(scream_imgui PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    IMGUI_IMPL_WIN32_DISABLE_GAMEPAD
    "IMGUI_API=__declspec(dllexport)"
)
set_target_properties(scream_imgui PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

set(SCREAMSDK_SOURCES
    src/Bootstrap.cpp
    src/Build.cpp
    src/Log.cpp
    src/Memory.cpp
    src/Plugins.cpp
    src/Diesel/Core.cpp
    src/Diesel/Input.cpp
    src/Diesel/Renderer.cpp
    src/Diesel/Resource.cpp
    src/Hooks/GameHooks.cpp
    src/Hooks/HookManager.cpp
    src/Hooks/LuaHooks.cpp
    src/Hooks/RendererHooks.cpp
    src/Lua/LuaCatalog.cpp
    src/Lua/LuaRuntime.cpp
    src/Overlay/DebugTabs.cpp
    src/Overlay/Overlay.cpp
)

add_library(ScreamSDK SHARED ${SCREAMSDK_SOURCES})
target_include_directories(ScreamSDK
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_definitions(ScreamSDK
    PRIVATE SCREAMSDK_EXPORTS WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS
)
target_link_libraries(ScreamSDK PRIVATE
    minhook
    scream_imgui
    d3d9
    advapi32
    comdlg32
    gdi32
    imm32
    user32
)
set_target_properties(ScreamSDK PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    OUTPUT_NAME "ScreamSDK"
)

if(MSVC)
    foreach(target scream_imgui ScreamSDK)
        target_compile_options(${target} PRIVATE /W4 /MP /utf-8)
    endforeach()
    if(MSVC_VERSION GREATER_EQUAL 1910)
        target_compile_options(ScreamSDK PRIVATE /permissive-)
    endif()
else()
    target_compile_options(ScreamSDK PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS ScreamSDK
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/ DESTINATION include)
install(FILES
    "${IMGUI_ROOT}/imgui.h"
    "${IMGUI_ROOT}/imconfig.h"
    DESTINATION include
)
