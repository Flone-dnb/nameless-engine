cmake_minimum_required(VERSION 3.12)

project(engine_lib)

# target
set(ENGINE_LIB_SOURCES
    private/Application.cpp
    public/Application.h
    private/Error.cpp
    private/Error.h
    private/Window.cpp
    private/Window.h
    private/UniqueValueGenerator.h
    private/UniqueValueGenerator.cpp
    # add your .h/.cpp files here
    PARENT_SCOPE
)