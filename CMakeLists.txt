cmake_minimum_required(VERSION 3.20)

project(engine_editor)

enable_testing()

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
add_subdirectory(src/engine_lib dependency_build/engine_lib)
target_link_libraries(${PROJECT_NAME} PRIVATE engine_lib)

# set standard
set(PROJECT_CXX_STANDARD_VERSION 23)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_${PROJECT_CXX_STANDARD_VERSION})

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ${ENGINE_FOLDER})

# doxygen
find_package(Doxygen)
if (DOXYGEN_FOUND)
    add_custom_target(doc_doxygen ALL
        COMMAND doxygen
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/docs/
        COMMENT "Generating API documentation with Doxygen..."
        VERBATIM )
    add_dependencies(doc_doxygen ${PROJECT_NAME} engine_lib)
else (DOXYGEN_FOUND)
  message(WARNING "Doxygen need to be installed to generate the documentation!")
endif (DOXYGEN_FOUND)

include_directories(ext)
include_directories(src/engine_editor)
include_directories(src/engine_lib/public)
include_directories(src/engine_lib/private)
