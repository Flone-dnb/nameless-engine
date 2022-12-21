cmake_minimum_required(VERSION 3.20)

project(engine_lib)

# See if Go is installed.
execute_process(
    COMMAND go version
    OUTPUT_VARIABLE go_out
    RESULT_VARIABLE go_error
    ERROR_VARIABLE go_suppress)
if (go_error)
    message(FATAL_ERROR "\"Go\" programming language needs to be installed.")
endif()

# Relative path to 'ext' directory.
set(RELATIVE_EXT_PATH "../../ext")

# Windows specific sources.
set(ENGINE_WIN32_SOURCES
    private/materials/hlsl/HlslShader.h
    private/materials/hlsl/HlslShader.cpp
    private/materials/hlsl/HlslEngineShaders.hpp
    private/materials/hlsl/RootSignatureGenerator.h
    private/materials/hlsl/RootSignatureGenerator.cpp
    private/render/directx/DirectXRenderer.h
    private/render/directx/DirectXRenderer.cpp
    private/render/directx/pso/DirectXPso.cpp
    private/render/directx/pso/DirectXPso.h
    private/render/directx/descriptors/DirectXDescriptorHeap.h
    private/render/directx/descriptors/DirectXDescriptorHeap.cpp
    private/render/directx/descriptors/DirectXDescriptor.h
    private/render/directx/descriptors/DirectXDescriptor.cpp
    private/render/directx/resources/DirectXResource.h
    private/render/directx/resources/DirectXResource.cpp
    private/render/directx/resources/DirectXResourceManager.h
    private/render/directx/resources/DirectXResourceManager.cpp
    # add Windows specific sources here...
)

# Prepare file that will include all generated reflection.
set(REFLECTION_FILE_NAME Reflection_impl.hpp)
set(REFLECTION_FILE_NAME ${REFLECTION_FILE_NAME} PARENT_SCOPE)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../.generated)
set(GENERATED_REFLECTION_FILE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../.generated/${REFLECTION_FILE_NAME})
if (NOT EXISTS ${GENERATED_REFLECTION_FILE_PATH})
    file(WRITE ${GENERATED_REFLECTION_FILE_PATH} "")
endif()

# Temporary solution for `tgc2` in `Refureku` (because we use it below).
set(TGC_CPP_FILE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/Refureku/build/Include/tgc2/include/tgc2.cpp)
set(TGC_H_FILE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/Refureku/build/Include/tgc2/include/tgc2.h)
if (NOT EXISTS ${TGC_CPP_FILE_PATH})
    file(WRITE ${TGC_CPP_FILE_PATH} "")
endif()

# Engine general (cross-platform) sources.
set(ENGINE_LIB_SOURCES
    ../.generated/${REFLECTION_FILE_NAME}
    ${RELATIVE_EXT_PATH}/xxHash/xxhash.c
    ${RELATIVE_EXT_PATH}/xxHash/xxhash.h
    private/misc/Error.cpp
    public/misc/Error.h
    private/misc/UniqueValueGenerator.h
    private/misc/UniqueValueGenerator.cpp
    private/window/Window.cpp
    public/game/Window.h
    private/game/GameInstance.cpp
    public/game/GameInstance.h
    private/game/Game.h
    private/game/Game.cpp
    public/render/Renderer.h
    private/render/Renderer.cpp
    private/render/pso/PsoManager.h
    private/render/pso/PsoManager.cpp
    private/render/pso/Pso.h
    private/render/pso/Pso.cpp
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
    public/materials/ShaderManager.h
    private/materials/ShaderManager.cpp
    private/materials/Shader.cpp
    private/materials/Shader.h
    public/materials/ShaderDescription.h
    private/materials/ShaderDescription.cpp
    private/materials/ShaderUser.h
    private/materials/ShaderUser.cpp
    private/materials/Material.cpp
    public/materials/Material.h
    private/materials/EngineShaderNames.hpp
    private/misc/ThreadPool.h
    private/misc/ThreadPool.cpp
    private/misc/Timer.cpp
    public/misc/Timer.h
    private/misc/MessageBox.cpp
    public/misc/MessageBox.h
    private/misc/DesktopNotification.cpp
    public/misc/DesktopNotification.h
    private/misc/OpenLinkInBrowser.cpp
    public/misc/OpenLinkInBrowser.h
    private/misc/FileDialog.cpp
    public/misc/FileDialog.h
    private/materials/ShaderPack.h
    private/materials/ShaderPack.cpp
    private/materials/ShaderParameter.h
    private/materials/ShaderParameter.cpp
    private/materials/ShaderFilesystemPaths.hpp
    public/misc/ProjectPaths.h
    private/misc/ProjectPaths.cpp
    private/io/Serializable.cpp
    public/io/Serializable.h
    private/game/nodes/Node.cpp
    public/game/nodes/Node.h
    private/game/nodes/SpatialNode.cpp
    public/game/nodes/SpatialNode.h
    private/game/nodes/MeshNode.cpp
    public/game/nodes/MeshNode.h
    public/io/SerializeProperty.h
    private/io/SerializeProperty.cpp
    private/io/GuidProperty.cpp
    public/io/GuidProperty.h
    private/game/World.h
    private/game/World.cpp
    public/misc/GC.hpp
    public/io/IFieldSerializer.hpp
    public/io/serializers/PrimitiveFieldSerializer.h
    private/io/serializers/PrimitiveFieldSerializer.cpp
    public/io/serializers/StringFieldSerializer.h
    private/io/serializers/StringFieldSerializer.cpp
    public/io/serializers/VectorFieldSerializer.h
    private/io/serializers/VectorFieldSerializer.cpp
    private/io/serializers/UnorderedMapFieldSerializer.cpp
    public/io/serializers/UnorderedMapFieldSerializer.h
    private/io/serializers/SerializableObjectFieldSerializer.cpp
    public/io/serializers/SerializableObjectFieldSerializer.h
    public/math/MathHelpers.hpp
    public/math/GLMath.hpp
    # add general (cross-platform) sources here...
)

# Append Window specific sources.
if (WIN32)
    message(STATUS "Appending Windows specific source files.")
    list(APPEND ENGINE_LIB_SOURCES ${ENGINE_WIN32_SOURCES})
else()
    message(STATUS "Windows specific source files are excluded.")
endif()

# Target.
add_library(${PROJECT_NAME} STATIC ${ENGINE_LIB_SOURCES})

# Set build mode.
set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Using build mode '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
endif()
message(STATUS "${PROJECT_NAME}: build type: ${CMAKE_BUILD_TYPE}.")

# Add DEBUG macro in Debug build.
if(CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    set(BUILD_MODE_DIRECTORY ${CMAKE_BINARY_DIR}/Debug)
    message(STATUS "Adding DEBUG macro for this build type.")
    add_compile_definitions(DEBUG)
endif()

# Add BUILD_MODE_DIRECTORY variable.
if(CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    set(BUILD_MODE_DIRECTORY ${CMAKE_BINARY_DIR}/Debug)
else()
    set(BUILD_MODE_DIRECTORY ${CMAKE_BINARY_DIR}/Release)
endif()

# Set folder.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(ENGINE_FOLDER "NAMELESS-ENGINE")
set(EXTERNAL_FOLDER "EXTERNAL")
set(EXTERNAL_FOLDER ${EXTERNAL_FOLDER} PARENT_SCOPE)
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ${ENGINE_FOLDER})

# More warnings.
if(MSVC)
    target_compile_options(${PROJECT_NAME} PUBLIC /W3 /WX)
else()
    target_compile_options(${PROJECT_NAME} PUBLIC -Wall -Wextra -Werror -Wno-unused-parameter)
endif()

# Compiler specific flags.
if(MSVC)
    target_compile_options(${PROJECT_NAME} PUBLIC /utf-8)
else()
    target_compile_options(${PROJECT_NAME} PUBLIC -Wno-unused-variable)
endif()

# Set C++ standard.
set(PROJECT_CXX_STANDARD_VERSION 23)
set(CMAKE_CXX_STANDARD ${PROJECT_CXX_STANDARD_VERSION})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_${PROJECT_CXX_STANDARD_VERSION})

# Enable Address Sanitizer.
if(CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message(STATUS "Adding Address Sanitizer flags.")
        set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
        set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    endif()
endif()

# Set directory name for dependencies.
set(DEPENDENCY_BUILD_DIR_NAME dependency_build)

# External: glfw.
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE) # disable docs
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE) # disable tests
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # disable examples
add_subdirectory(${RELATIVE_EXT_PATH}/glfw ${DEPENDENCY_BUILD_DIR_NAME}/glfw) # add GLFW target
target_link_libraries(${PROJECT_NAME} PUBLIC glfw)
set_target_properties(glfw PROPERTIES FOLDER ${EXTERNAL_FOLDER})

# External: spdlog.
add_subdirectory(${RELATIVE_EXT_PATH}/spdlog ${DEPENDENCY_BUILD_DIR_NAME}/spdlog)
target_link_libraries(${PROJECT_NAME} PUBLIC spdlog::spdlog)
set_target_properties(spdlog PROPERTIES FOLDER ${EXTERNAL_FOLDER})

# External: toml11.
if (MSVC)
    target_compile_options(${PROJECT_NAME} PUBLIC /Zc:__cplusplus)
endif()
add_subdirectory(${RELATIVE_EXT_PATH}/toml11 ${DEPENDENCY_BUILD_DIR_NAME}/toml11)
set_target_properties(toml11 PROPERTIES FOLDER ${EXTERNAL_FOLDER})

# External: portable-file-dialogs
add_subdirectory(${RELATIVE_EXT_PATH}/portable-file-dialogs ${DEPENDENCY_BUILD_DIR_NAME}/portable-file-dialogs)
target_link_libraries(${PROJECT_NAME} PUBLIC portable_file_dialogs)
set_target_properties(portable_file_dialogs PROPERTIES FOLDER ${EXTERNAL_FOLDER})

# External: fmt
add_subdirectory(${RELATIVE_EXT_PATH}/fmt ${DEPENDENCY_BUILD_DIR_NAME}/fmt)
target_link_libraries(${PROJECT_NAME} PUBLIC fmt)
set_target_properties(fmt PROPERTIES FOLDER ${EXTERNAL_FOLDER})

# External: GLM
add_subdirectory(${RELATIVE_EXT_PATH}/glm ${DEPENDENCY_BUILD_DIR_NAME}/glm)
target_link_libraries(${PROJECT_NAME} PUBLIC glm)
set_target_properties(glm PROPERTIES FOLDER ${EXTERNAL_FOLDER})

# [temporary solution]: External: tgc
target_sources(${PROJECT_NAME} PUBLIC ${TGC_CPP_FILE_PATH})
if(NOT MSVC)
    message(STATUS "Adding flags to ignore some warnings in external dependency `tgc2`.")
    set_source_files_properties(${TGC_CPP_FILE_PATH} PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-error=extra")
    set_source_files_properties(${TGC_H_FILE_PATH} PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-error=extra")
endif()

# Doxygen
option(ENABLE_DOXYGEN "Enables doxygen for controling and generating the documentation." ON)
if (ENABLE_DOXYGEN)
    find_package(Doxygen)
    if (DOXYGEN_FOUND)
        set(DOXYGEN_TARGET doc_doxygen)
        add_custom_target(${DOXYGEN_TARGET}
            COMMAND doxygen
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../docs/
            COMMENT "Generating API documentation using Doxygen..."
            VERBATIM )
        set_target_properties(${DOXYGEN_TARGET} PROPERTIES FOLDER ${EXTERNAL_FOLDER})
        add_dependencies(${PROJECT_NAME} ${DOXYGEN_TARGET})
        message("Doxygen is enabled.")
    else()
        message(FATAL "Doxygen need to be installed to control the documentation, please install Doxygen.")
    endif()
else()
    message(WARNING "Doxygen is disabled.")
endif()

# External: Refureku (start) --------------------------------------------------------------------------
# Check that path does not have pipe character because we use it to separate paths.
string(FIND ${CMAKE_CURRENT_SOURCE_DIR} "|" PIPE_POS)
if (NOT ${PIPE_POS} STREQUAL "-1")
    message(FATAL_ERROR "Path ${CMAKE_CURRENT_SOURCE_DIR} contains forbidden character '|'.")
endif()

# Write project include directories for Refureku.
set(REFUREKU_INCLUDE_DIRECTORIES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../ext/|\
${CMAKE_CURRENT_SOURCE_DIR}/../../ext/glfw/include|\
${CMAKE_CURRENT_SOURCE_DIR}/../../ext/DirectX-Headers/include|\
${CMAKE_CURRENT_SOURCE_DIR}/../../ext/spdlog/include|\
${CMAKE_CURRENT_SOURCE_DIR}/../../ext/source_location/include|\
${CMAKE_CURRENT_SOURCE_DIR}/../../ext/fmt/include|\
${CMAKE_CURRENT_SOURCE_DIR}/../../ext/glm|\
${CMAKE_CURRENT_SOURCE_DIR}/../.generated|\
${CMAKE_CURRENT_SOURCE_DIR}/public/|\
${CMAKE_CURRENT_SOURCE_DIR}/private|${CMAKE_CURRENT_SOURCE_DIR}/../../ext/Refureku/build/Include/")

# Collect files to exclude from analyzing.
set(REFUREKU_EXCLUDE_FILES "")
if (NOT WIN32)
    # Add Windows sources to be excluded from analyzing.
    foreach(SRC_PATH IN LISTS ENGINE_WIN32_SOURCES)
        set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${SRC_PATH})
        string(FIND ${SRC_PATH} "|" PIPE_POS)
        if (NOT ${PIPE_POS} STREQUAL "-1")
            message(FATAL_ERROR "Path ${SRC_PATH} contains forbidden character '|'.")
        endif()
        set(REFUREKU_EXCLUDE_FILES ${SRC_PATH}|${REFUREKU_EXCLUDE_FILES})
    endforeach()
endif()

# Define "Setup Refureku" target.
set(SETUP_REFUREKU_TARGET SetupRefureku)
add_custom_target(${SETUP_REFUREKU_TARGET}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/Refureku/
                COMMAND go run ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/Refureku/download_and_setup_refureku.go
                ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/Refureku/ # working directory
                ${CMAKE_CURRENT_SOURCE_DIR}/../ # source directory (where to put Refureku settings file)
                "\"${REFUREKU_INCLUDE_DIRECTORIES}\"" # project include directories
                "\"${REFUREKU_EXCLUDE_FILES}\"" # files to exclude from analyzing
                ${CMAKE_CXX_COMPILER_ID} # used compiler
)
add_dependencies(${PROJECT_NAME} ${SETUP_REFUREKU_TARGET})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/Refureku/build/Include/)
set_target_properties(${SETUP_REFUREKU_TARGET} PROPERTIES FOLDER ${EXTERNAL_FOLDER})

# Link Refureku library.
if (MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ignore:4006")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /ignore:4006")
    set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /ignore:4006")
endif()
if (WIN32)
    target_link_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/Refureku/build/Lib)
elseif(APPLE) # test macos before unix
    message(FATAL_ERROR "Put Refureku dynamic library directory here.")
elseif(UNIX)
    target_link_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/Refureku/build/Bin)
endif()
target_link_libraries(${PROJECT_NAME} PUBLIC Refureku)

# Add a pre build step to run reflection code generator.
set(REFUREKU_GENERATOR_TARGET RunRefurekuGeneratorEngine)
add_custom_target(${REFUREKU_GENERATOR_TARGET}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/Refureku/build/Bin/RefurekuGenerator ${CMAKE_CURRENT_SOURCE_DIR}/../.reflection/RefurekuSettings.toml
        COMMAND go run ${CMAKE_CURRENT_SOURCE_DIR}/../.reflection/merge_generated_reflection/merge_generated_reflection.go ${CMAKE_CURRENT_SOURCE_DIR}/../.generated ${REFLECTION_FILE_NAME}
)
add_dependencies(${REFUREKU_GENERATOR_TARGET} ${SETUP_REFUREKU_TARGET})
add_dependencies(${PROJECT_NAME} ${REFUREKU_GENERATOR_TARGET})
set_target_properties(${REFUREKU_GENERATOR_TARGET} PROPERTIES FOLDER ${EXTERNAL_FOLDER})

# Add generated headers to include directories.
target_include_directories(${PROJECT_NAME} PUBLIC ../.generated)

# Copy Refureku dynamic library to the top level binary directory.
set(REFUREKU_DYN_LIB Refureku.dll)
if (NOT WIN32)
    if(APPLE) # test macos before unix
        message(FATAL_ERROR "Put Refureku dynamic library name here.")
    elseif(UNIX)
        set(REFUREKU_DYN_LIB libRefureku.so)
    endif()
endif()
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/Refureku/build/Bin/${REFUREKU_DYN_LIB} ${CMAKE_BINARY_DIR}/${REFUREKU_DYN_LIB})

# Copy Refureku dynamic library to the current top level build directory.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/Refureku/build/Bin/${REFUREKU_DYN_LIB} ${BUILD_MODE_DIRECTORY}/${REFUREKU_DYN_LIB})
# External: Refureku (end) --------------------------------------------------------------------------

# Win32 specific dependencies:
if (WIN32)
    # External: D3D12MemoryAllocator
    add_subdirectory(${RELATIVE_EXT_PATH}/D3D12MemoryAllocator ${DEPENDENCY_BUILD_DIR_NAME}/D3D12MemoryAllocator)
    target_link_libraries(${PROJECT_NAME} PUBLIC D3D12MemoryAllocator)
    set_target_properties(D3D12MemoryAllocator PROPERTIES FOLDER ${EXTERNAL_FOLDER})

    # External: DirectX-Headers
    add_subdirectory(${RELATIVE_EXT_PATH}/DirectX-Headers ${DEPENDENCY_BUILD_DIR_NAME}/DirectX-Headers)
    target_link_libraries(${PROJECT_NAME} PUBLIC DirectX-Headers)
    set_target_properties(DirectX-Headers PROPERTIES FOLDER ${EXTERNAL_FOLDER})
    set_target_properties(DirectX-Guids PROPERTIES FOLDER ${EXTERNAL_FOLDER})

    # External: DXC.
    # Download and unzip DXC.
    set(SETUP_DXT_TARGET SetupDirectXShaderCompiler)
    add_custom_target(${SETUP_DXT_TARGET}
                   COMMAND go run ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/DirectXShaderCompiler/download_dxc.go
                   ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/DirectXShaderCompiler/ # working directory
    )
    add_dependencies(${PROJECT_NAME} ${SETUP_DXT_TARGET})
    set_target_properties(${SETUP_DXT_TARGET} PROPERTIES FOLDER ${EXTERNAL_FOLDER})

    # Set DXC variables.
    set(PATH_TO_DXC_DLL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/DirectXShaderCompiler/bin/x64)
    set(PATH_TO_DXC_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/DirectXShaderCompiler/lib/x64)

    # Link to DXC libraries.
    target_link_directories(${PROJECT_NAME} PUBLIC ${PATH_TO_DXC_DLL_DIR})
    target_link_directories(${PROJECT_NAME} PUBLIC ${PATH_TO_DXC_LIB_DIR})

    # Copy DXC DLLs to the top level binary directory.
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   ${PATH_TO_DXC_DLL_DIR}/dxcompiler.dll ${CMAKE_BINARY_DIR}/dxcompiler.dll)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   ${PATH_TO_DXC_DLL_DIR}/dxil.dll ${CMAKE_BINARY_DIR}/dxil.dll)

    # Copy DXC DLLs to the current top level build directory.
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   ${PATH_TO_DXC_DLL_DIR}/dxcompiler.dll ${BUILD_MODE_DIRECTORY}/dxcompiler.dll)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   ${PATH_TO_DXC_DLL_DIR}/dxil.dll ${BUILD_MODE_DIRECTORY}/dxil.dll)

    # Copy DXC DLLs to engine_lib binary directory.
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   ${PATH_TO_DXC_DLL_DIR}/dxcompiler.dll ${CMAKE_CURRENT_BINARY_DIR}/dxcompiler.dll)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   ${PATH_TO_DXC_DLL_DIR}/dxil.dll ${CMAKE_CURRENT_BINARY_DIR}/dxil.dll)
endif()

# Execute post build script.
if(CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    set(IS_RELEASE_BUILD 0)
else()
    set(IS_RELEASE_BUILD 1)
endif()
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND go run ${CMAKE_CURRENT_LIST_DIR}/engine_post_build.go
                   ${CMAKE_CURRENT_LIST_DIR}/../../res/
                   ${CMAKE_CURRENT_LIST_DIR}/../../ext/
                   ${CMAKE_BINARY_DIR}
                   ${CMAKE_CURRENT_BINARY_DIR}
                   ${BUILD_MODE_DIRECTORY}
                   ${IS_RELEASE_BUILD}
)

# Tests.
add_subdirectory(${RELATIVE_EXT_PATH}/Catch2 ${DEPENDENCY_BUILD_DIR_NAME}/Catch2)
set_target_properties(Catch2 PROPERTIES FOLDER ${EXTERNAL_FOLDER})
set_target_properties(Catch2WithMain PROPERTIES FOLDER ${EXTERNAL_FOLDER})

set(ENGINE_TESTS_NAME engine_tests)
set(TESTS_RELATIVE_PATH ../../tests)

set(ENGINE_TEST_WIN32_SOURCES
    ${TESTS_RELATIVE_PATH}/engine_lib/render/directx/DirectXResourceManager.cpp
    # add Windows specific test sources here...
)

set(ENGINE_TEST_SOURCES
    ${TESTS_RELATIVE_PATH}/engine_lib.cpp
    ${TESTS_RELATIVE_PATH}/engine_lib/game/Window.cpp
    ${TESTS_RELATIVE_PATH}/engine_lib/io/ConfigManager.cpp
    ${TESTS_RELATIVE_PATH}/engine_lib/io/ReflectionTest.h
    ${TESTS_RELATIVE_PATH}/engine_lib/input/InputManager.cpp
    ${TESTS_RELATIVE_PATH}/engine_lib/misc/Timer.cpp
    ${TESTS_RELATIVE_PATH}/engine_lib/materials/ShaderPack.cpp
    ${TESTS_RELATIVE_PATH}/engine_lib/materials/Material.cpp
    ${TESTS_RELATIVE_PATH}/engine_lib/io/Serializable.cpp
    ${TESTS_RELATIVE_PATH}/engine_lib/game/nodes/Node.cpp
    ${TESTS_RELATIVE_PATH}/engine_lib/game/nodes/SpatialNode.cpp
    ${TESTS_RELATIVE_PATH}/engine_lib/game/World.cpp
    ${TESTS_RELATIVE_PATH}/engine_lib/game/GameInstance.cpp
    # add engine test .cpp files here
)

if (WIN32)
    list(APPEND ENGINE_TEST_SOURCES ${ENGINE_TEST_WIN32_SOURCES})
endif()

add_executable(${ENGINE_TESTS_NAME} ${ENGINE_TEST_SOURCES})

target_compile_features(${ENGINE_TESTS_NAME} PRIVATE cxx_std_${PROJECT_CXX_STANDARD_VERSION})

target_link_libraries(${ENGINE_TESTS_NAME} PRIVATE engine_lib)
target_link_libraries(${ENGINE_TESTS_NAME} PRIVATE Catch2::Catch2)

set_target_properties(${ENGINE_TESTS_NAME} PROPERTIES FOLDER ${ENGINE_FOLDER})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/ext/Catch2/contrib")
include(CTest)
include(Catch)
#catch_discover_tests(${ENGINE_TESTS_NAME})

# Add includes.
target_include_directories(${PROJECT_NAME} PUBLIC ${RELATIVE_EXT_PATH})
target_include_directories(${PROJECT_NAME} PUBLIC ${RELATIVE_EXT_PATH}/source_location/include)
target_include_directories(${PROJECT_NAME} PUBLIC public)
target_include_directories(${PROJECT_NAME} PUBLIC private)

message(STATUS "${PROJECT_NAME} is configured to use the following C++ standard: ${CMAKE_CXX_STANDARD}")
