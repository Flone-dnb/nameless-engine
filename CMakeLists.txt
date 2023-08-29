cmake_minimum_required(VERSION 3.20)

project(nameless)

# Set build directory name.
set(BUILD_DIRECTORY_NAME OUTPUT)

# Add executable targets.
message(STATUS "Adding executable targets...")

# Add editor target.
set(EDITOR_DIRECTORY editor)
message(STATUS "Adding target ${EDITOR_DIRECTORY}...")
add_subdirectory(src/${EDITOR_DIRECTORY} ${BUILD_DIRECTORY_NAME}/${EDITOR_DIRECTORY})

# Add engine tests target.
set(ENGINE_TESTS_DIRECTORY engine_tests)
message(STATUS "Adding target ${ENGINE_TESTS_DIRECTORY}...")
add_subdirectory(src/${ENGINE_TESTS_DIRECTORY} ${BUILD_DIRECTORY_NAME}/${ENGINE_TESTS_DIRECTORY})
