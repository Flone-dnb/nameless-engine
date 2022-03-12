cmake_minimum_required(VERSION 3.12)

project(engine_lib)

# target
set(ENGINE_LIB_SOURCES
    private/misc/Error.cpp
    private/misc/Error.h
    private/window/Window.cpp
    public/Window.h
    private/misc/UniqueValueGenerator.h
    private/misc/UniqueValueGenerator.cpp
    private/game/IGameInstance.cpp
    public/IGameInstance.h
    private/game/Game.h
    private/game/Game.cpp
    private/render/IRenderer.h
    private/render/IRenderer.cpp
    private/render/DirectXRenderer.h
    private/render/DirectXRenderer.cpp
    private/window/GLFW.hpp
    private/input/KeyboardKey.hpp
    private/misc/Logger.cpp
    public/Logger.h
    private/misc/Globals.h
    private/misc/Globals.cpp
    # add your .h/.cpp files here
    PARENT_SCOPE
)