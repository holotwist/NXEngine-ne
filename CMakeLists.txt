cmake_minimum_required(VERSION 3.22)
project(nxengine-ne LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# Platform Configuration
option(BUILD_EXTRACTOR "Build the exe data extractor tool" ON)

include(FetchContent)

set(SUPPORT_MODULE_RAUDIO OFF CACHE BOOL "Disable Raylib internal audio" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Disable Raylib examples" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "Disable Raylib games" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
)
FetchContent_MakeAvailable(raylib)

# Global Include Directories
set(ENGINE_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/deps
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/ai
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/common
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/i18n
    ${CMAKE_CURRENT_SOURCE_DIR}/src/formats
    ${CMAKE_CURRENT_SOURCE_DIR}/src/formats/siflib
    ${CMAKE_CURRENT_SOURCE_DIR}/src/formats/autogen
    ${CMAKE_CURRENT_SOURCE_DIR}/src/graphics
    ${CMAKE_CURRENT_SOURCE_DIR}/src/input
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sound
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
)

# Target Asset Extractor
if(BUILD_EXTRACTOR AND NOT ANDROID)
    file(GLOB EXTRACT_SRC
        "src/extract/*.cpp"
        "src/core/common/misc.cpp"
    )
    add_executable(nxextract ${EXTRACT_SRC})
    target_include_directories(nxextract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/deps
        ${CMAKE_CURRENT_SOURCE_DIR}/src/extract
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/common
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/utils
    )
    target_compile_definitions(nxextract PRIVATE EXTRACTOR_TOOL)
endif()

# Target NXEngine-ne

# Core Logic
file(GLOB_RECURSE CORE_SRC
    "src/core/*.cpp"
)
# Exclude extract main and any extract sources if caught
list(FILTER CORE_SRC EXCLUDE REGEX "src/extract/.*")

# Formats
file(GLOB_RECURSE FORMATS_SRC
    "src/formats/*.cpp"
)

# Graphics
file(GLOB_RECURSE GRAPHICS_SRC
    "src/graphics/*.cpp"
)

# Input
file(GLOB_RECURSE INPUT_SRC
    "src/input/*.cpp"
)

# Platform
file(GLOB_RECURSE PLATFORM_SRC
    "src/platform/*.cpp"
)

# Audio Engine
file(GLOB_RECURSE SOUND_SRC
    "src/sound/*.cpp"
)

# UI
file(GLOB_RECURSE UI_SRC
    "src/ui/*.cpp"
)

set(ALL_GAME_SOURCES
    ${CORE_SRC}
    ${FORMATS_SRC}
    ${GRAPHICS_SRC}
    ${INPUT_SRC}
    ${PLATFORM_SRC}
    ${SOUND_SRC}
    ${UI_SRC}
)

add_executable(nxengine-ne ${ALL_GAME_SOURCES})

target_include_directories(nxengine-ne PRIVATE ${ENGINE_INCLUDES})

# Raylib link
target_link_libraries(nxengine-ne PRIVATE raylib)

# Platform Link Flags
if(UNIX AND NOT ANDROID)
    target_link_libraries(nxengine-ne PRIVATE
        m
        pthread
        dl
    )
endif()

if(ANDROID)
    target_link_libraries(nxengine-ne PRIVATE
        android
        log
        OpenSLES
        GLESv2
        EGL
        m
    )
endif()

# Asset Packaging
if(NOT ANDROID)
    add_custom_command(TARGET nxengine-ne POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/data
        $<TARGET_FILE_DIR:nxengine-ne>/data
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:nxengine-ne>/resources
        COMMENT "Syncing data and resources to output folder..."
    )
endif()