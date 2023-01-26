cmake_minimum_required(VERSION 3.20)

project(engine_editor)

enable_testing()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(ENGINE_FOLDER "NAMELESS-ENGINE")

# Set default build mode.
set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Using build mode '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
endif()
message(STATUS "${PROJECT_NAME}: build type: ${CMAKE_BUILD_TYPE}.")

# Add DEBUG macro for Debug builds.
if(CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    message(STATUS "Adding DEBUG macro for this build type.")
    add_compile_definitions(DEBUG)
endif()

# Add `WIN32` macro for debug builds (some setups don't define it).
if(WIN32)
    message(STATUS "Adding WIN32 macro because running Windows.")
    add_compile_definitions(WIN32)
endif()

# Add `_WIN32` macro for non MSVC compiler (needed for some third party dependencies).
if(WIN32 AND NOT MSVC)
    add_compile_definitions(_WIN32)
endif()

# Create some path variables.
set(EDITOR_SRC_RELATIVE_PATH src/engine_editor)
set(EDITOR_SRC_ABSOLUTE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${EDITOR_SRC_RELATIVE_PATH})
set(PATH_TO_EXT ${CMAKE_CURRENT_SOURCE_DIR}/ext)

# Define target.
set(PROJECT_SOURCES
    ${EDITOR_SRC_RELATIVE_PATH}/EditorGameInstance.h
    ${EDITOR_SRC_RELATIVE_PATH}/EditorGameInstance.cpp
    # add your .h/.cpp files here
)
add_executable(${PROJECT_NAME}
    ${EDITOR_SRC_RELATIVE_PATH}/main.cpp
    ${PROJECT_SOURCES}
)
target_include_directories(${PROJECT_NAME} PRIVATE ${EDITOR_SRC_RELATIVE_PATH})

# Set application type.
if (MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

# Add engine.
set(ENGINE_TARGET engine_lib)
add_subdirectory(src/${ENGINE_TARGET} dependency_build/${ENGINE_TARGET})
target_link_libraries(${PROJECT_NAME} PRIVATE ${ENGINE_TARGET})
add_dependencies(${PROJECT_NAME} ${ENGINE_TARGET})

# Set C++ standard.
set(PROJECT_CXX_STANDARD_VERSION 23)
set(CMAKE_CXX_STANDARD ${PROJECT_CXX_STANDARD_VERSION})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_${PROJECT_CXX_STANDARD_VERSION})

# Set target folder.
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ${ENGINE_FOLDER})

# Enable Address Sanitizer.
if(CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    if(NOT WIN32)
        message(STATUS "Adding Address Sanitizer flags.")
        set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
        set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    endif()
endif()

message(STATUS "${PROJECT_NAME} is configured to use the following C++ standard: ${CMAKE_CXX_STANDARD}")
