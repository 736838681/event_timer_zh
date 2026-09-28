cmake_minimum_required(VERSION 3.24)
project(EventTimerCN LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Keep Nexus API and ImGui revisions pinned so the build is reproducible.
FetchContent_Declare(
    nexus_api
    GIT_REPOSITORY https://github.com/RaidcoreGG/Nexus-API.git
    GIT_TAG 9b2c53df86c00db6495642bfcff2d0611bd957ef
)
FetchContent_MakeAvailable(nexus_api)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/RaidcoreGG/imgui18000.git
    GIT_TAG 58075c4414b985b352d10718b02a8c43f25efd7c
)
FetchContent_MakeAvailable(imgui)

add_library(EventTimerCN SHARED
    src/entry.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)

target_include_directories(EventTimerCN PRIVATE
    ${nexus_api_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)



if(MSVC)
    target_compile_definitions(EventTimerCN PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(EventTimerCN PRIVATE /utf-8 /W4 /permissive-)
    set_property(TARGET EventTimerCN PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set_target_properties(EventTimerCN PROPERTIES
    OUTPUT_NAME "EventTimerCN"
    PREFIX ""
)
