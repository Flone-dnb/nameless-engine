cmake_minimum_required(VERSION 3.12)

project(engine_editor)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(ENGINE_FOLDER "NAMELESS-ENGINE")

# build mode
set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE)
    message(WARNING "Using build mode '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
endif()
message(WARNING "${PROJECT_NAME}: build type: ${CMAKE_BUILD_TYPE}.")

# add DEBUG macro in Debug build
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(WARNING "Adding DEBUG macro for this build type.")
    add_compile_definitions(DEBUG)
endif()

# application type
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")

# add engine_lib
add_subdirectory(src/engine_lib)
list(TRANSFORM ENGINE_LIB_SOURCES PREPEND "src/engine_lib/")

# target
set(PROJECT_SOURCES
    ${ENGINE_LIB_SOURCES}
    src/engine_editor/EditorGameInstance.h
    src/engine_editor/EditorGameInstance.cpp
    # add your .h/.cpp files here
)
add_executable(${PROJECT_NAME}
    src/engine_editor/main.cpp
    ${PROJECT_SOURCES}
)
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ${ENGINE_FOLDER})

# more warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3 /WX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)
endif()

# set standard
set(PROJECT_CXX_STANDARD_VERSION 20)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_${PROJECT_CXX_STANDARD_VERSION})

# glfw
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE) # disable docs
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE) # disable tests
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # disable examples
add_subdirectory(ext/glfw build/glfw) # add GLFW target
target_link_libraries(${PROJECT_NAME} glfw)

# tests
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_subdirectory(ext/Catch2 build/Catch2) # add Catch2 target
    add_executable(tests
        ${PROJECT_SOURCES}
        tests/main.cpp
        tests/engine_lib/Window.cpp
        # add your test .cpp files here
    )
    target_compile_features(tests PRIVATE cxx_std_${PROJECT_CXX_STANDARD_VERSION})
    target_link_libraries(tests PRIVATE Catch2::Catch2)
    target_link_libraries(tests PRIVATE glfw)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/ext/Catch2/contrib")
    include(CTest)
    include(Catch)
    catch_discover_tests(tests)
    set_target_properties(tests PROPERTIES FOLDER ${ENGINE_FOLDER})
endif()

include_directories(ext)
include_directories(src/engine_editor)
include_directories(src/engine_lib/public)
include_directories(src/engine_lib/private)