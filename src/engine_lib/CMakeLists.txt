cmake_minimum_required(VERSION 3.12)

project(engine_lib)

# target
set(ENGINE_LIB_SOURCES
    private/Error.cpp
    private/Error.h
    private/Window.cpp
    public/Window.h
    private/UniqueValueGenerator.h
    private/UniqueValueGenerator.cpp
    private/IGameInstance.cpp
    public/IGameInstance.h
    private/Game.h
    private/Game.cpp
    private/IRenderer.h
    private/IRenderer.cpp
    private/DirectXRenderer.h
    private/DirectXRenderer.cpp
    # add your .h/.cpp files here
    PARENT_SCOPE
)