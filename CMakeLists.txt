cmake_minimum_required(VERSION 3.22)
project(nxengine-ne LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# Platform Configuration
option(BUILD_EXTRACTOR "Build the exe data extractor tool" ON)

# Raylib resolution
set(RAYLIB_PATH "" CACHE PATH "Path to local Raylib build or installation root")

if(RAYLIB_PATH)
    message(STATUS "Using user-specified Raylib at: ${RAYLIB_PATH}")
    find_library(RAYLIB_LIB NAMES raylib PATHS "${RAYLIB_PATH}/lib" "${RAYLIB_PATH}" NO_DEFAULT_PATH)
    find_path(RAYLIB_INCLUDE_DIR NAMES raylib.h PATHS "${RAYLIB_PATH}/include" "${RAYLIB_PATH}" NO_DEFAULT_PATH)

    if(RAYLIB_LIB AND RAYLIB_INCLUDE_DIR)
        add_library(raylib UNKNOWN IMPORTED)
        set_target_properties(raylib PROPERTIES
            IMPORTED_LOCATION "${RAYLIB_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${RAYLIB_INCLUDE_DIR}"
        )
    else()
        message(FATAL_ERROR "Could not find raylib library or header in: ${RAYLIB_PATH}")
    endif()
else()
    find_package(raylib 4.5 QUIET)
    if(NOT raylib_FOUND)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(RAYLIB REQUIRED raylib)
        add_library(raylib UNKNOWN IMPORTED)
        set_target_properties(raylib PROPERTIES
            IMPORTED_LOCATION "${RAYLIB_LINK_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${RAYLIB_INCLUDE_DIRS}"
        )
    endif()
endif()

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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/TextBox
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/intro
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/pause
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/endgame
)

# Target Asset Extractor
if(BUILD_EXTRACTOR AND NOT ANDROID)
    file(GLOB EXTRACT_SRC
        "src/extract/*.cpp"
        "src/core/common/misc.cpp"
        "src/core/utils/Logger.cpp"
        "src/formats/stagedata.cpp"
    )
    add_executable(nxextract ${EXTRACT_SRC})
    target_include_directories(nxextract PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/deps
        ${CMAKE_CURRENT_SOURCE_DIR}/src/extract
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/common
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/utils
        ${CMAKE_CURRENT_SOURCE_DIR}/src/formats
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