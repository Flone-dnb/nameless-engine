# File that should be included by all engine-related `CMakeLists` files.

# Set default build mode to be `Release`.
set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Using build mode '${DEFAULT_BUILD_TYPE}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
endif()
message(STATUS "${PROJECT_NAME}: build type: ${CMAKE_BUILD_TYPE}.")

# Prepare variable for build type.
if(CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    set(IS_RELEASE_BUILD 0)
else()
    set(IS_RELEASE_BUILD 1)
endif()

# Add `DEBUG` macro in debug builds.
if(NOT IS_RELEASE_BUILD)
    message(STATUS "Adding DEBUG macro for this build type.")
    add_compile_definitions(DEBUG)
endif()

# Add `BUILD_MODE_DIRECTORY` variable.
if(NOT IS_RELEASE_BUILD)
    set(BUILD_MODE_DIRECTORY ${CMAKE_BINARY_DIR}/Debug)
else()
    set(BUILD_MODE_DIRECTORY ${CMAKE_BINARY_DIR}/Release)
endif()
message(STATUS "Current build directory: ${BUILD_MODE_DIRECTORY}.")

# Add `WIN32` macro on Windows (some setups don't define it).
if(WIN32 AND NOT MSVC)
    message(STATUS "Adding WIN32 macro because running Windows on non-MSVS compiler.")
    add_compile_definitions(WIN32)
endif()

# Add `_WIN32` macro for Windows on non MSVC compiler (needed for some third party dependencies).
if(WIN32 AND NOT MSVC)
    message(STATUS "Adding _WIN32 macro because running Windows on non-MSVS compiler.")
    add_compile_definitions(_WIN32)
endif()

# Set directory name for dependencies.
set(DEPENDENCY_BUILD_DIR_NAME dependency_build)

# Enable cmake folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Define some folder names.
set(ENGINE_FOLDER "NAMELESS-ENGINE")
set(EXTERNAL_FOLDER "EXTERNAL")