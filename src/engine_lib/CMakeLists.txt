cmake_minimum_required(VERSION 3.12)

project(engine_lib CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(ENGINE_FOLDER "NAMELESS-ENGINE")

# target
set(ENGINE_LIB_SOURCES
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
    private/render/directx/DirectXRenderer.h
    private/render/directx/DirectXRenderer.cpp
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
    # add your .h/.cpp files here
)

add_library(${PROJECT_NAME} STATIC ${ENGINE_LIB_SOURCES})

# build mode
set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Using build mode '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
endif()
message(STATUS "${PROJECT_NAME}: build type: ${CMAKE_BUILD_TYPE}.")

# add DEBUG macro in Debug build
if(CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    message(STATUS "Adding DEBUG macro for this build type.")
    add_compile_definitions(DEBUG)
endif()

# set folder
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ${ENGINE_FOLDER})

# more warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PUBLIC /W3 /WX)
else()
    target_compile_options(${PROJECT_NAME} PUBLIC -Wall -Wextra -Werror)
endif()

# set standard
set(PROJECT_CXX_STANDARD_VERSION 23)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_${PROJECT_CXX_STANDARD_VERSION})

# glfw
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE) # disable docs
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE) # disable tests
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # disable examples
add_subdirectory(../../ext/glfw ../../build/glfw) # add GLFW target
target_link_libraries(${PROJECT_NAME} PUBLIC glfw)

# spdlog
add_subdirectory(../../ext/spdlog ../../build/spdlog)
target_link_libraries(${PROJECT_NAME} PUBLIC spdlog::spdlog)

include_directories(../../ext)
include_directories(public)
include_directories(private)