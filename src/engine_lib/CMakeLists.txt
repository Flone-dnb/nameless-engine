cmake_minimum_required(VERSION 3.12)

project(engine_lib)

# target
set(ENGINE_LIB_SOURCES
    private/misc/Error.cpp
    private/misc/Error.h
    private/Window.cpp
    public/Window.h
    private/misc/UniqueValueGenerator.h
    private/misc/UniqueValueGenerator.cpp
    private/IGameInstance.cpp
    public/IGameInstance.h
    private/Game.h
    private/Game.cpp
    private/render/IRenderer.h
    private/render/IRenderer.cpp
    private/render/DirectXRenderer.h
    private/render/DirectXRenderer.cpp
    private/GLFW.hpp
    private/KeyboardKey.hpp
    # add your .h/.cpp files here
    PARENT_SCOPE
)