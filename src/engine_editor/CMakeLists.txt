cmake_minimum_required(VERSION 3.20)

project(engine_editor)

enable_testing()

# Define some relative paths.
set(RELATIVE_EXT_PATH "../../ext")
set(RELATIVE_CMAKE_HELPERS_PATH "../.cmake")

# Include essential stuff.
include(${RELATIVE_CMAKE_HELPERS_PATH}/essential.cmake)

# Include helper functions.
include(${RELATIVE_CMAKE_HELPERS_PATH}/utils.cmake)

# Specify project sources.
set(PROJECT_SOURCES
    private/EditorGameInstance.h
    private/EditorGameInstance.cpp
    # add your .h/.cpp files here
)

# Define target.
add_executable(${PROJECT_NAME}
    private/main.cpp
    ${PROJECT_SOURCES}
)

# Add includes.
target_include_directories(${PROJECT_NAME} PUBLIC public)
target_include_directories(${PROJECT_NAME} PUBLIC private)

# Enable more warnings and warnings as errors.
enable_more_warnings(${PROJECT_NAME})

# Set application type.
if (MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

# Add engine.
set(ENGINE_TARGET engine_lib)
message(STATUS "${PROJECT_NAME}: started adding ${ENGINE_TARGET}...\n----------------------------------------------\n")
add_subdirectory(../${ENGINE_TARGET} dependency_build/${ENGINE_TARGET})
target_link_libraries(${PROJECT_NAME} PRIVATE ${ENGINE_TARGET})
add_dependencies(${PROJECT_NAME} ${ENGINE_TARGET})
message(STATUS "\n\n----------------------------------------------\n${PROJECT_NAME}: finished adding ${ENGINE_TARGET}")

# Set C++ standard.
set(PROJECT_CXX_STANDARD_VERSION 23)
set(CMAKE_CXX_STANDARD ${PROJECT_CXX_STANDARD_VERSION})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_${PROJECT_CXX_STANDARD_VERSION})

# Set target folder.
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ${ENGINE_FOLDER})

# Enable Address Sanitizer in `Debug` builds on non-Windows OS.
if(NOT IS_RELEASE_BUILD AND NOT WIN32)
    enable_address_sanitizer(${PROJECT_NAME})
endif()

# Setup Clang-tidy in release builds.
if(IS_RELEASE_BUILD)
    find_program (CLANG_TIDY NAMES "clang-tidy" REQUIRED)
    set(RUN_CLANG_TIDY
        "${CLANG_TIDY}"
        "--header-filter=src/* --config-file=${CMAKE_CURRENT_LIST_DIR}/.clang-tidy")
    set_target_properties(${PROJECT_NAME} PROPERTIES CXX_CLANG_TIDY "${RUN_CLANG_TIDY}")
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PUBLIC -EHsc) # needed for clang-tidy when running MSVC to enable exceptions
    endif()
    message(STATUS "${PROJECT_NAME}: Clang-tidy is enabled.")
endif()

# Enable doxygen.
enable_doxygen(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/../../docs)

message(STATUS "${PROJECT_NAME}: using the following C++ standard: ${CMAKE_CXX_STANDARD}")
