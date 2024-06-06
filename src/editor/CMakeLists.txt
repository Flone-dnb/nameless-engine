cmake_minimum_required(VERSION 3.20)

project(editor)

# Define some relative paths.
set(RELATIVE_EXT_PATH "../../ext")
set(RELATIVE_CMAKE_HELPERS_PATH "../.cmake")
set(RELATIVE_SCRIPTS_PATH "../.scripts")

# Include essential stuff.
include(${RELATIVE_CMAKE_HELPERS_PATH}/essential.cmake)

# Include helper functions.
include(${RELATIVE_CMAKE_HELPERS_PATH}/utils.cmake)

# -------------------------------------------------------------------------------------------------
#                                          TARGET SOURCES
# -------------------------------------------------------------------------------------------------

# Specify project sources.
set(PROJECT_SOURCES
    src/main.cpp
    # add your .h/.cpp files here
)

# Define target.
if (WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${PROJECT_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
endif()

# -------------------------------------------------------------------------------------------------
#                                         CONFIGURE TARGET
# -------------------------------------------------------------------------------------------------

if (WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

# Set target folder.
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ${ENGINE_FOLDER})

# Enable more warnings and warnings as errors.
enable_more_warnings()

# Set C++ standard.
set(PROJECT_CXX_STANDARD_VERSION 23)
set(CMAKE_CXX_STANDARD ${PROJECT_CXX_STANDARD_VERSION})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_${PROJECT_CXX_STANDARD_VERSION})
message(STATUS "${PROJECT_NAME}: using the following C++ standard: ${CMAKE_CXX_STANDARD}")

# Add includes.
target_include_directories(${PROJECT_NAME} PUBLIC public)
target_include_directories(${PROJECT_NAME} PUBLIC private)

# Add a post build script.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/${RELATIVE_SCRIPTS_PATH}/${POST_BUILD_SCRIPT_NAME}
                   COMMAND go run ${CMAKE_CURRENT_LIST_DIR}/${RELATIVE_SCRIPTS_PATH}/${POST_BUILD_SCRIPT_NAME}/${POST_BUILD_SCRIPT_NAME}.go
                   ${CMAKE_CURRENT_LIST_DIR}/../../res/
                   ${CMAKE_CURRENT_LIST_DIR}/../../ext/
                   ${CMAKE_BINARY_DIR}
                   ${CMAKE_CURRENT_BINARY_DIR}
                   ${IS_RELEASE_BUILD}
                   ${CMAKE_CURRENT_LIST_DIR}/${RELATIVE_SCRIPTS_PATH}/${DELETE_NONGAME_FILES_SCRIPT_NAME}
)

# -------------------------------------------------------------------------------------------------
#                                           TOOLS
# -------------------------------------------------------------------------------------------------

# Enable Address Sanitizer in `Debug` builds on non-Windows OS.
if(NOT IS_RELEASE_BUILD AND NOT WIN32)
    enable_address_sanitizer()
endif()

# Enable doxygen.
enable_doxygen(${CMAKE_CURRENT_SOURCE_DIR}/../../docs)

# -------------------------------------------------------------------------------------------------
#                                       DEPENDENCIES
# -------------------------------------------------------------------------------------------------

# Add editor library.
set(EDITOR_LIB_TARGET editor_lib)
if (NOT TARGET ${EDITOR_LIB_TARGET}) # define target only if not defined yet
    message(STATUS "${PROJECT_NAME}: started adding ${EDITOR_LIB_TARGET}...\n----------------------------------------------\n")
    add_subdirectory(../${EDITOR_LIB_TARGET} ${DEPENDENCY_BUILD_DIR_NAME}/${EDITOR_LIB_TARGET})
    message(STATUS "\n\n----------------------------------------------\n${PROJECT_NAME}: finished adding ${EDITOR_LIB_TARGET}")
else()
    message(STATUS "${PROJECT_NAME}: ${EDITOR_LIB_TARGET} already defined, just using it without redefining")
endif()
target_link_libraries(${PROJECT_NAME} PUBLIC ${EDITOR_LIB_TARGET})
add_dependencies(${PROJECT_NAME} ${EDITOR_LIB_TARGET})
