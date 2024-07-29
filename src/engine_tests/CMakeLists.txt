cmake_minimum_required(VERSION 3.20)

project(engine_tests)

# Define some paths.
set(RELATIVE_EXT_PATH "../../ext")
set(ABSOLUTE_EXT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${RELATIVE_EXT_PATH})
set(RELATIVE_CMAKE_HELPERS_PATH "../.cmake")
set(RELATIVE_SCRIPTS_PATH "../.scripts")

# Include essential stuff.
include(${RELATIVE_CMAKE_HELPERS_PATH}/essential.cmake)

# Include helper functions.
include(${RELATIVE_CMAKE_HELPERS_PATH}/utils.cmake)

# -------------------------------------------------------------------------------------------------
#                                          TARGET SOURCES
# -------------------------------------------------------------------------------------------------

set(PROJECT_WIN32_SOURCES
    src/render/directx/DirectXDescriptorHeap.cpp
    src/shader/hlsl/RootSignatureGenerator.cpp
    # add Windows specific test files here...
)

set(PROJECT_SOURCES
    src/main.cpp
    src/TestHelpers.hpp
    src/io/ReflectionTest.h
    src/io/ReflectionTest.cpp
    src/game/Window.cpp
    src/io/ConfigManager.cpp
    src/input/InputManager.cpp
    src/misc/Timer.cpp
    src/shader/general/ShaderPack.cpp
    src/material/Material.cpp
    src/shader/general/ShaderManager.cpp
    src/material/TextureManager.cpp
    src/shader/glsl/DescriptorSetLayoutGenerator.cpp
    src/shader/general/LightingShaderResourceManager.cpp
    src/io/Serializable.cpp
    src/render/RenderSettings.cpp
    src/shader/general/DynamicCpuWriteShaderResourceArray.cpp
    src/render/Pipeline.cpp
    src/render/PipelineManager.cpp
    src/math/GLMath.cpp
    src/math/MathHelpers.cpp
    src/game/GameManager.cpp
    src/game/callbacks/NodeFunction.cpp
    src/game/callbacks/NodeNotificationBroadcaster.cpp
    src/game/nodes/Node.cpp
    src/game/nodes/SpatialNode.cpp
    src/game/nodes/MeshNode.cpp
    src/game/nodes/CameraNode.cpp
    src/game/nodes/LightNodes.cpp
    src/game/nodes/EnvironmentNode.cpp
    src/game/camera/CameraManager.cpp
    src/game/World.cpp
    src/game/GameInstance.cpp
    # add general (cross-platform) test files here...
)

# Append Windows specific sources if we are on Windows.
if (WIN32)
    list(APPEND PROJECT_SOURCES ${PROJECT_WIN32_SOURCES})
endif()

# Define target.
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

# -------------------------------------------------------------------------------------------------
#                                         CONFIGURE TARGET
# -------------------------------------------------------------------------------------------------

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
target_include_directories(${PROJECT_NAME} PUBLIC src)

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

# -------------------------------------------------------------------------------------------------
#                                       DEPENDENCIES
# -------------------------------------------------------------------------------------------------

# Add engine library.
set(ENGINE_LIB_TARGET engine_lib)
if (NOT TARGET ${ENGINE_LIB_TARGET}) # define target only if not defined yet
    message(STATUS "${PROJECT_NAME}: started adding ${ENGINE_LIB_TARGET}...\n----------------------------------------------\n")
    add_subdirectory(../${ENGINE_LIB_TARGET} ${DEPENDENCY_BUILD_DIR_NAME}/${ENGINE_LIB_TARGET})
    message(STATUS "\n\n----------------------------------------------\n${PROJECT_NAME}: finished adding ${ENGINE_LIB_TARGET}")
else()
    message(STATUS "${PROJECT_NAME}: ${ENGINE_LIB_TARGET} already defined, just using it without redefining")
endif()
target_link_libraries(${PROJECT_NAME} PUBLIC ${ENGINE_LIB_TARGET})
add_dependencies(${PROJECT_NAME} ${ENGINE_LIB_TARGET})

# External: Refureku
set(REFUREKU_EXCLUDED_FILES "")
if (NOT WIN32)
    set(REFUREKU_EXCLUDED_FILES PROJECT_WIN32_SOURCES)
endif()
# Write project include directories for Refureku.
set(REFUREKU_INCLUDE_DIRECTORIES
    ${ABSOLUTE_EXT_PATH}/Catch2/src/catch2
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
add_refureku(
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${REFUREKU_EXCLUDED_FILES}"
    "${ABSOLUTE_EXT_PATH}/Refureku"
    "${ENGINE_LIB_TARGET}"
    "${REFUREKU_INCLUDE_DIRECTORIES}"
)

# Add `Catch2`.
message(STATUS "${PROJECT_NAME}: adding external dependency \"Catch2\"...")
if (NOT TARGET Catch2) # define target only if not defined yet
    add_subdirectory(${RELATIVE_EXT_PATH}/Catch2 ${DEPENDENCY_BUILD_DIR_NAME}/Catch2 SYSTEM)
endif()
set_target_properties(Catch2 PROPERTIES FOLDER ${EXTERNAL_FOLDER})
set_target_properties(Catch2WithMain PROPERTIES FOLDER ${EXTERNAL_FOLDER})
target_link_libraries(${PROJECT_NAME} PUBLIC Catch2::Catch2)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/ext/Catch2/contrib")
include(CTest)
include(Catch)
