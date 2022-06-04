cmake_minimum_required(VERSION 3.20)

project(engine_lib CXX)

# See if Go is installed.
execute_process(
    COMMAND go version
    OUTPUT_VARIABLE go_out
    RESULT_VARIABLE go_error
    ERROR_VARIABLE go_suppress)
if (go_error)
    message(FATAL_ERROR "\"Go\" programming language needs to be installed.")
endif()

# Relative path to 'ext' directory.
set(RELATIVE_EXT_PATH "../../ext")

# Windows specific sources.
set(ENGINE_WIN32_SOURCES
    private/shaders/hlsl/HlslShader.h
    private/shaders/hlsl/HlslShader.cpp
    private/render/directx/DirectXRenderer.h
    private/render/directx/DirectXRenderer.cpp
    private/render/directx/Pso.cpp
    private/render/directx/Pso.h
    # add Windows specific sources here...
)

# Engine general sources.
set(ENGINE_LIB_SOURCES
    ${RELATIVE_EXT_PATH}/xxHash/xxhash.c
    ${RELATIVE_EXT_PATH}/xxHash/xxhash.h
    private/misc/Error.cpp
    private/misc/Error.h
    private/window/Window.cpp
    public/game/Window.h
    private/misc/UniqueValueGenerator.h
    private/misc/UniqueValueGenerator.cpp
    private/game/IGameInstance.cpp
    public/game/IGameInstance.h
    private/game/Game.h
    private/game/Game.cpp
    private/render/IRenderer.h
    private/render/IRenderer.cpp
    private/window/GLFW.hpp
    public/input/KeyboardKey.hpp
    private/io/Logger.cpp
    public/io/Logger.h
    private/misc/Globals.h
    private/misc/Globals.cpp
    private/io/ConfigManager.cpp
    public/io/ConfigManager.h
    private/input/InputManager.cpp
    public/input/InputManager.h
    public/input/MouseButton.hpp
    public/shaders/ShaderManager.h
    private/shaders/ShaderManager.cpp
    private/shaders/IShader.cpp
    private/shaders/IShader.h
    public/shaders/ShaderDescription.h
    private/shaders/ShaderDescription.cpp
    private/shaders/ShaderUser.h
    private/shaders/ShaderUser.cpp
    private/misc/ThreadPool.h
    private/misc/ThreadPool.cpp
    # add general (cross-platform) sources here...
)

if (WIN32)
    list(APPEND ENGINE_LIB_SOURCES ${ENGINE_WIN32_SOURCES})
endif()

# Target.
add_library(${PROJECT_NAME} STATIC ${ENGINE_LIB_SOURCES})

# Set build mode.
set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Using build mode '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
endif()
message(STATUS "${PROJECT_NAME}: build type: ${CMAKE_BUILD_TYPE}.")

# Add DEBUG macro in Debug build.
if(CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    set(BUILD_MODE_DIRECTORY ${CMAKE_BINARY_DIR}/Debug)
    message(STATUS "Adding DEBUG macro for this build type.")
    add_compile_definitions(DEBUG)
endif()

# Add BUILD_MODE_DIRECTORY variable.
if(CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    set(BUILD_MODE_DIRECTORY ${CMAKE_BINARY_DIR}/Debug)
else()
    set(BUILD_MODE_DIRECTORY ${CMAKE_BINARY_DIR}/Release)
endif()

# Set folder.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(ENGINE_FOLDER "NAMELESS-ENGINE")
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ${ENGINE_FOLDER})

# More warnings.
if(MSVC)
    target_compile_options(${PROJECT_NAME} PUBLIC /W3 /WX)
else()
    target_compile_options(${PROJECT_NAME} PUBLIC -Wall -Wextra -Werror)
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PUBLIC /utf-8)
endif()

# Set C++ standard.
set(PROJECT_CXX_STANDARD_VERSION 23)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_${PROJECT_CXX_STANDARD_VERSION})

# Copy engine resources.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                   ${CMAKE_CURRENT_SOURCE_DIR}/../../res/engine/ ${CMAKE_BINARY_DIR}/res/engine/)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                   ${CMAKE_CURRENT_SOURCE_DIR}/../../res/engine/ ${BUILD_MODE_DIRECTORY}/res/engine/)

# External: glfw.
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE) # disable docs
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE) # disable tests
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # disable examples
add_subdirectory(${RELATIVE_EXT_PATH}/glfw ../../build/glfw) # add GLFW target
target_link_libraries(${PROJECT_NAME} PUBLIC glfw)

# External: spdlog.
add_subdirectory(${RELATIVE_EXT_PATH}/spdlog ../../build/spdlog)
target_link_libraries(${PROJECT_NAME} PUBLIC spdlog::spdlog)

# External: toml11.
if (MSVC)
    target_compile_options(${PROJECT_NAME} PUBLIC /Zc:__cplusplus)
endif()
add_subdirectory(${RELATIVE_EXT_PATH}/toml11 build/toml11)

if (WIN32)
    # External: D3D12MemoryAllocator
    add_subdirectory(${RELATIVE_EXT_PATH}/D3D12MemoryAllocator build/D3D12MemoryAllocator)
    target_link_libraries(${PROJECT_NAME} PUBLIC D3D12MemoryAllocator)

    # External: DirectX-Headers
    add_subdirectory(${RELATIVE_EXT_PATH}/DirectX-Headers build/DirectX-Headers)
    target_link_libraries(${PROJECT_NAME} PUBLIC DirectX-Headers)

    # External: DXC.
    # Download and unzip DXC.
    add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
                   COMMAND go run ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/DirectXShaderCompiler/download_dxc.go
                   ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/DirectXShaderCompiler/ # working directory
    )

    set(PATH_TO_DXC_DLL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/DirectXShaderCompiler/bin/x64)
    set(PATH_TO_DXC_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/DirectXShaderCompiler/lib/x64)
    target_link_directories(${PROJECT_NAME} PUBLIC ${PATH_TO_DXC_DLL_DIR})
    target_link_directories(${PROJECT_NAME} PUBLIC ${PATH_TO_DXC_LIB_DIR})

    # Copy DXC DLLs.
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   ${PATH_TO_DXC_DLL_DIR}/dxcompiler.dll ${CMAKE_BINARY_DIR}/dxcompiler.dll)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   ${PATH_TO_DXC_DLL_DIR}/dxil.dll ${CMAKE_BINARY_DIR}/dxil.dll)

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   ${PATH_TO_DXC_DLL_DIR}/dxcompiler.dll ${BUILD_MODE_DIRECTORY}/dxcompiler.dll)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   ${PATH_TO_DXC_DLL_DIR}/dxil.dll ${BUILD_MODE_DIRECTORY}/dxil.dll)
endif()

# Copy external licenses to build directory.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND go run ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/copy_ext_licenses.go
                   ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/ # arg 1 - working directory
                   ${BUILD_MODE_DIRECTORY} # arg 2 - build directory
)

# Add includes.
include_directories(${RELATIVE_EXT_PATH})
include_directories(public)
include_directories(private)