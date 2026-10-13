add_executable(ScreamBundleTool
    src/main.cpp
    src/ArchiveTool.cpp
)

if(MSVC AND MSVC_VERSION LESS 1910)
    set(SCREAM_BUNDLE_CXX_STANDARD 14)
else()
    set(SCREAM_BUNDLE_CXX_STANDARD 17)
endif()

set_target_properties(ScreamBundleTool PROPERTIES
    CXX_STANDARD ${SCREAM_BUNDLE_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_include_directories(ScreamBundleTool PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(ScreamBundleTool PRIVATE scream_zlib)

if(MINGW)
    target_link_options(ScreamBundleTool PRIVATE -static-libgcc -static-libstdc++)
    get_filename_component(MINGW_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    add_custom_command(TARGET ScreamBundleTool POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${MINGW_RUNTIME_DIR}/libwinpthread-1.dll"
                "$<TARGET_FILE_DIR:ScreamBundleTool>/libwinpthread-1.dll"
        VERBATIM)
endif()

if(MSVC)
    target_compile_options(ScreamBundleTool PRIVATE /W4 /EHsc)
    if(MSVC_VERSION GREATER_EQUAL 1910)
        target_compile_options(ScreamBundleTool PRIVATE /permissive-)
    endif()
else()
    target_compile_options(ScreamBundleTool PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS ScreamBundleTool RUNTIME DESTINATION bin)
