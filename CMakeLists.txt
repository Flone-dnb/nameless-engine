cmake_minimum_required(VERSION 3.12)

project(engine_editor CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(ENGINE_FOLDER "NAMELESS-ENGINE")

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

# application type
if (WIN32)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif(WIN32)

# target
set(PROJECT_SOURCES
    src/engine_editor/EditorGameInstance.h
    src/engine_editor/EditorGameInstance.cpp
    # add your .h/.cpp files here
)
add_executable(${PROJECT_NAME}
    src/engine_editor/main.cpp
    ${PROJECT_SOURCES}
)

# add engine_lib
add_subdirectory(src/engine_lib)
target_link_libraries(${PROJECT_NAME} PRIVATE engine_lib)

# set standard
set(PROJECT_CXX_STANDARD_VERSION 23)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_${PROJECT_CXX_STANDARD_VERSION})

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ${ENGINE_FOLDER})

# copy resources to build folder
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                   ${CMAKE_SOURCE_DIR}/res/editor/ $<TARGET_FILE_DIR:${PROJECT_NAME}>/res/editor/)
# and to the directory above for less hassle
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                   ${CMAKE_SOURCE_DIR}/res/editor/ $<TARGET_FILE_DIR:${PROJECT_NAME}>/../res/editor/)

# tests
if(CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    add_subdirectory(ext/Catch2 build/Catch2) # add Catch2 target
    add_executable(tests
        ${PROJECT_SOURCES}
        tests/main.cpp
        tests/engine_lib/Window.cpp
        tests/engine_lib/ConfigManager.cpp
        tests/engine_lib/InputManager.cpp
        # add your test .cpp files here
    )
    target_link_libraries(tests PRIVATE engine_lib)
    target_compile_features(tests PRIVATE cxx_std_${PROJECT_CXX_STANDARD_VERSION})
    target_link_libraries(tests PRIVATE Catch2::Catch2)
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
