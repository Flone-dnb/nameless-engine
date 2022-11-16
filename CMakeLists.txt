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

# Add engine.
set(ENGINE_TARGET engine_lib)
add_subdirectory(src/${ENGINE_TARGET} dependency_build/${ENGINE_TARGET})
target_link_libraries(${PROJECT_NAME} PRIVATE ${ENGINE_TARGET})
add_dependencies(${PROJECT_NAME} ${ENGINE_TARGET})
# Add a pre build step to run reflection code generator.
# set(REFUREKU_GENERATOR_TARGET RunRefurekuGeneratorEditor)
# add_custom_target(${REFUREKU_GENERATOR_TARGET}
#         WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
#         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/ext/Refureku/build/Bin/RefurekuGenerator ${CMAKE_CURRENT_SOURCE_DIR}/src/.reflection/RefurekuSettings.toml
#         COMMAND go run ${CMAKE_CURRENT_SOURCE_DIR}/src/.reflection/merge_generated_reflection/merge_generated_reflection.go ${CMAKE_CURRENT_SOURCE_DIR}/src/.generated ${REFLECTION_FILE_NAME}
# )
# add_dependencies(${REFUREKU_GENERATOR_TARGET} ${ENGINE_TARGET})
# add_dependencies(${PROJECT_NAME} ${REFUREKU_GENERATOR_TARGET})
# set_target_properties(${REFUREKU_GENERATOR_TARGET} PROPERTIES FOLDER ${EXTERNAL_FOLDER})

# set standard
set(PROJECT_CXX_STANDARD_VERSION 23)
set(CMAKE_CXX_STANDARD ${PROJECT_CXX_STANDARD_VERSION})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_${PROJECT_CXX_STANDARD_VERSION})

# set folder
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ${ENGINE_FOLDER})

# clang-tidy
find_program (CLANG_TIDY NAMES "clang-tidy")
if(CLANG_TIDY)
    message(STATUS "Found clang-tidy at ${CLANG_TIDY}")
    set(CMAKE_CXX_CLANG_TIDY "clang-tidy;--config-file=${CMAKE_CURRENT_LIST_DIR}/.clang-tidy")
    # because standard version is not exported with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        if("${PROJECT_CXX_STANDARD_VERSION}" STREQUAL "23")
            target_compile_options(${PROJECT_NAME} PUBLIC /std:c++latest)
        else()
            target_compile_options(${PROJECT_NAME} PUBLIC /std:c++${PROJECT_CXX_STANDARD_VERSION})
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${PROJECT_NAME} PUBLIC --std=c++${PROJECT_CXX_STANDARD_VERSION})
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        if("${PROJECT_CXX_STANDARD_VERSION}" STREQUAL "23")
            target_compile_options(${PROJECT_NAME} PUBLIC -std=c++2b)
        else()
            target_compile_options(${PROJECT_NAME} PUBLIC -std=c++${PROJECT_CXX_STANDARD_VERSION})
        endif()
    endif()
else()
    message(FATAL "clang-tidy not found")
    set(CMAKE_CXX_CLANG_TIDY "" CACHE STRING "" FORCE)
endif()

# Enable Address Sanitizer.
if(CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message(STATUS "Adding Address Sanitizer flags.")
        set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
        set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    endif()
endif()

message(STATUS "${PROJECT_NAME} is configured to use the following C++ standard: ${CMAKE_CXX_STANDARD}")
