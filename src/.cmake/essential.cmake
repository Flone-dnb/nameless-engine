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
    message(STATUS "${PROJECT_NAME}: adding DEBUG macro for this build type.")
    add_compile_definitions(DEBUG)
endif()

# Add `WIN32` and `_WIN32` macros on Windows (some setups don't define them).
if(WIN32)
    add_compile_definitions(WIN32)
    add_compile_definitions(_WIN32)
endif()

# Set directory name for dependencies.
set(DEPENDENCY_BUILD_DIR_NAME dependency_build)

# Enable cmake folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Define some folder names.
set(ENGINE_FOLDER "NAMELESS-ENGINE")
set(EXTERNAL_FOLDER "EXTERNAL")
