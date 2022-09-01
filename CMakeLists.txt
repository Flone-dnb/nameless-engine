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

# path variables
set(EDITOR_SRC_RELATIVE_PATH src/engine_editor)
set(EDITOR_SRC_ABSOLUTE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${EDITOR_SRC_RELATIVE_PATH})
set(PATH_TO_EXT ${CMAKE_CURRENT_SOURCE_DIR}/ext)

# target
set(PROJECT_SOURCES
    ${EDITOR_SRC_RELATIVE_PATH}/EditorGameInstance.h
    ${EDITOR_SRC_RELATIVE_PATH}/EditorGameInstance.cpp
    # add your .h/.cpp files here
)
add_executable(${PROJECT_NAME}
    ${EDITOR_SRC_RELATIVE_PATH}/main.cpp
    ${PROJECT_SOURCES}
)
target_include_directories(${PROJECT_NAME} PRIVATE ${EDITOR_SRC_RELATIVE_PATH})

# add engine_lib
add_subdirectory(src/engine_lib dependency_build/engine_lib)
target_link_libraries(${PROJECT_NAME} PRIVATE engine_lib)
add_dependencies(${PROJECT_NAME} engine_lib)
# Add a pre build step to run reflection code generator.
add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
	WORKING_DIRECTORY ${EDITOR_SRC_ABSOLUTE_PATH}/../
	COMMAND ${PATH_TO_EXT}/Refureku/build/Bin/RefurekuGenerator ${EDITOR_SRC_ABSOLUTE_PATH}/../.reflection/RefurekuSettings.toml
    COMMAND go run ${EDITOR_SRC_ABSOLUTE_PATH}/../.reflection/merge_generated_reflection/merge_generated_reflection.go ${EDITOR_SRC_ABSOLUTE_PATH}/../.generated ${REFLECTION_FILE_NAME})

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
