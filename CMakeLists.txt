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
