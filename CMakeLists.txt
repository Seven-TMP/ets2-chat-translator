cmake_minimum_required(VERSION 3.15)
project(ets2_chat_translator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "This project must be built as 64-bit (x64).")
endif()

set(PLUGIN_SOURCES
    src/dllmain.cpp
    src/app_runtime.cpp
    src/chat_panel.cpp
    src/chat_tailer.cpp
    src/translate_engine.cpp
    src/http_agent.cpp
    src/settings_store.cpp
    src/text_codec.cpp
    src/win_paths.cpp
)

set(PLUGIN_HEADERS
    include/scs_telemetry.h
    src/app_runtime.h
    src/chat_panel.h
    src/chat_tailer.h
    src/translate_engine.h
    src/http_agent.h
    src/settings_store.h
    src/core_types.h
    src/text_codec.h
    src/win_paths.h
)

add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES} ${PLUGIN_HEADERS} exports.def)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    user32
    gdi32
    winhttp
    shell32
    advapi32
    shlwapi
    crypt32
    imm32
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "ets2_chat_translator"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3 /O2 /utf-8)
endif()
