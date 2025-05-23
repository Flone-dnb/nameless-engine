cmake_minimum_required(VERSION 3.20)

project(engine_lib)

# Make sure we are running a 64-bit system.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "We don't support 32 bit systems.")
endif()

# Make sure Go is installed.
execute_process(
    COMMAND go version
    OUTPUT_VARIABLE go_out
    RESULT_VARIABLE go_error
    ERROR_VARIABLE go_suppress)
if (go_error)
    message(FATAL_ERROR "\"Go\" programming language needs to be installed.")
endif()

# Define some paths.
set(RELATIVE_EXT_PATH "../../ext")
set(ABSOLUTE_EXT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${RELATIVE_EXT_PATH})
set(RELATIVE_CMAKE_HELPERS_PATH "../.cmake")

# Include essential stuff.
include(${RELATIVE_CMAKE_HELPERS_PATH}/essential.cmake)

# Include helper functions.
include(${RELATIVE_CMAKE_HELPERS_PATH}/utils.cmake)

# -------------------------------------------------------------------------------------------------
#                                          TARGET SOURCES
# -------------------------------------------------------------------------------------------------

# Windows specific sources.
set(PROJECT_WIN32_SOURCES
    private/shader/hlsl/HlslShader.h
    private/shader/hlsl/HlslShader.cpp
    private/shader/hlsl/TextureSamplerDescriptions.hpp
    private/shader/hlsl/RootSignatureGenerator.h
    private/shader/hlsl/RootSignatureGenerator.cpp
    private/shader/hlsl/resource/binding/texture/HlslShaderTextureResourceBinding.h
    private/shader/hlsl/resource/binding/texture/HlslShaderTextureResourceBinding.cpp
    private/shader/hlsl/resource/binding/global/HlslGlobalShaderResourceBinding.cpp
    private/shader/hlsl/resource/binding/global/HlslGlobalShaderResourceBinding.h
    private/shader/hlsl/HlslComputeShaderInterface.cpp
    private/shader/hlsl/HlslComputeShaderInterface.h
    private/shader/hlsl/format/HlslVertexFormatDescription.h
    private/shader/hlsl/format/HlslVertexFormatDescription.cpp
    private/shader/hlsl/format/MeshNodeHlslVertexFormatDescription.h
    private/shader/hlsl/format/MeshNodeHlslVertexFormatDescription.cpp
    private/render/directx/DirectXRenderer.h
    private/render/directx/DirectXRenderer.cpp
    private/render/directx/pipeline/DirectXPso.cpp
    private/render/directx/pipeline/DirectXPso.h
    private/render/directx/descriptors/DirectXDescriptorHeap.h
    private/render/directx/descriptors/DirectXDescriptorHeap.cpp
    private/render/directx/descriptors/DirectXDescriptor.h
    private/render/directx/descriptors/DirectXDescriptor.cpp
    private/render/directx/descriptors/DirectXDescriptorType.hpp
    private/render/directx/resource/DirectXResource.h
    private/render/directx/resource/DirectXResource.cpp
    private/render/directx/resource/DirectXResourceManager.h
    private/render/directx/resource/DirectXResourceManager.cpp
    private/render/directx/resource/DirectXFrameResource.h
    private/render/directx/resource/DirectXFrameResource.cpp
    private/render/directx/resource/shadow/DirectXShadowMapArrayIndexManager.h
    private/render/directx/resource/shadow/DirectXShadowMapArrayIndexManager.cpp
    private/shader/hlsl/cache/HlslShaderCacheManager.cpp
    private/shader/hlsl/cache/HlslShaderCacheManager.h
    # add Windows specific sources here...
)

# Engine general (cross-platform) sources.
set(PROJECT_SOURCES
    private/misc/Error.cpp
    public/misc/Error.h
    private/misc/UniqueValueGenerator.h
    private/misc/UniqueValueGenerator.cpp
    private/misc/StdHashes.hpp
    private/window/Window.cpp
    public/game/Window.h
    private/game/GameInstance.cpp
    public/game/GameInstance.h
    private/game/GameManager.h
    private/game/GameManager.cpp
    public/render/Renderer.h
    private/render/general/Renderer.cpp
    public/render/RenderSettings.h
    private/render/general/RenderSettings.cpp
    public/render/RenderStatistics.h
    private/render/general/RenderStatistics.cpp
    private/render/general/pipeline/PipelineManager.h
    private/render/general/pipeline/PipelineManager.cpp
    private/render/general/pipeline/Pipeline.h
    private/render/general/pipeline/Pipeline.cpp
    private/render/general/pipeline/PipelineConfiguration.cpp
    private/render/general/pipeline/PipelineConfiguration.h
    private/render/general/pipeline/PipelineType.hpp
    private/render/general/pipeline/PipelineSharedPtr.h
    private/render/general/pipeline/PipelineSharedPtr.cpp
    private/render/general/pipeline/PipelineShaderConstantsManager.hpp
    private/render/general/pipeline/PipelineRegistry.hpp
    private/render/vulkan/pipeline/VulkanPipeline.h
    private/render/vulkan/pipeline/VulkanPipeline.cpp
    private/render/general/resource/GpuResource.h
    private/render/general/resource/GpuResource.cpp
    private/render/general/resource/MeshData.h
    private/render/general/resource/MeshData.cpp
    private/render/general/resource/frame/FrameResourceManager.h
    private/render/general/resource/frame/FrameResourceManager.cpp
    private/render/general/resource/frame/FrameResource.h
    private/render/general/resource/frame/FrameResource.cpp
    private/render/general/resource/shadow/ShadowMapManager.cpp
    private/render/general/resource/shadow/ShadowMapManager.h
    private/render/general/resource/shadow/ShadowMapHandle.cpp
    private/render/general/resource/shadow/ShadowMapHandle.h
    private/render/general/resource/shadow/ShadowMapType.hpp
    private/render/general/resource/shadow/ShadowMapArrayIndexManager.h
    private/render/general/resource/shadow/ShadowMapArrayIndexManager.cpp
    private/render/general/resource/GpuResourceManager.h
    private/render/general/resource/GpuResourceManager.cpp
    private/render/general/resource/UploadBuffer.h
    private/render/general/resource/UploadBuffer.cpp
    private/render/vulkan/resource/VulkanResourceManager.cpp
    private/render/vulkan/resource/VulkanResourceManager.h
    private/render/vulkan/resource/KtxLoadingCallbackManager.cpp
    private/render/vulkan/resource/KtxLoadingCallbackManager.h
    private/render/vulkan/resource/VulkanResource.cpp
    private/render/vulkan/resource/VulkanResource.h
    private/render/vulkan/VulkanRenderer.cpp
    private/render/vulkan/VulkanRenderer.h
    private/render/vulkan/resource/VulkanFrameResource.h
    private/render/vulkan/resource/VulkanFrameResource.cpp
    private/render/vulkan/resource/shadow/VulkanShadowMapArrayIndexManager.h
    private/render/vulkan/resource/shadow/VulkanShadowMapArrayIndexManager.cpp
    private/window/GLFW.hpp
    public/input/KeyboardKey.hpp
    private/io/Logger.cpp
    public/io/Logger.h
    private/io/FieldSerializerManager.cpp
    public/io/FieldSerializerManager.h
    public/misc/Globals.h
    private/misc/Globals.cpp
    private/io/ConfigManager.cpp
    public/io/ConfigManager.h
    private/io/MeshImporter.cpp
    public/io/MeshImporter.h
    private/io/TextureImporter.cpp
    public/io/TextureImporter.h
    private/input/InputManager.cpp
    public/input/InputManager.h
    public/input/MouseButton.hpp
    public/shader/ShaderManager.h
    private/shader/general/ShaderManager.cpp
    private/shader/general/format/VertexFormat.h
    private/shader/general/format/VertexFormat.cpp
    private/shader/general/cache/ShaderCacheManager.cpp
    private/shader/general/cache/ShaderCacheManager.h
    private/shader/general/resource/cpuwrite/DynamicCpuWriteShaderResourceArray.cpp
    private/shader/general/resource/cpuwrite/DynamicCpuWriteShaderResourceArray.h
    private/shader/general/resource/cpuwrite/DynamicCpuWriteShaderResourceArrayManager.cpp
    private/shader/general/resource/cpuwrite/DynamicCpuWriteShaderResourceArrayManager.h
    private/shader/general/resource/binding/cpuwrite/ShaderCpuWriteResourceBinding.cpp
    private/shader/general/resource/binding/cpuwrite/ShaderCpuWriteResourceBinding.h
    private/shader/general/resource/binding/global/GlobalShaderResourceBinding.cpp
    private/shader/general/resource/binding/global/GlobalShaderResourceBinding.h
    private/shader/general/resource/binding/global/GlobalShaderResourceBindingManager.cpp
    private/shader/general/resource/binding/global/GlobalShaderResourceBindingManager.h
    private/shader/glsl/format/GlslVertexFormatDescription.h
    private/shader/glsl/format/GlslVertexFormatDescription.cpp
    private/shader/glsl/format/MeshNodeGlslVertexFormatDescription.cpp
    private/shader/glsl/format/MeshNodeGlslVertexFormatDescription.h
    private/shader/glsl/cache/GlslShaderCacheManager.cpp
    private/shader/glsl/cache/GlslShaderCacheManager.h
    private/shader/general/DescriptorConstants.hpp
    private/shader/general/Shader.cpp
    private/shader/general/Shader.h
    public/shader/ShaderDescription.h
    private/shader/general/ShaderDescription.cpp
    private/shader/general/ShaderUser.h
    private/shader/general/ShaderUser.cpp
    private/material/Material.cpp
    public/material/Material.h
    private/shader/general/EngineShaderNames.hpp
    private/shader/general/EngineShaders.hpp
    private/shader/general/EngineShaderConstantMacros.hpp
    private/shader/general/resource/binding/ShaderResourceBinding.h
    private/shader/general/resource/binding/ShaderResourceBinding.cpp
    private/shader/general/resource/ShaderArrayIndexManager.h
    private/shader/general/resource/ShaderArrayIndexManager.cpp
    private/shader/general/resource/ShaderLightsInFrustumSingleType.hpp
    private/shader/glsl/GlslShader.h
    private/shader/glsl/GlslShader.cpp
    private/shader/glsl/resource/binding/texture/GlslShaderTextureResourceBinding.h
    private/shader/glsl/resource/binding/texture/GlslShaderTextureResourceBinding.cpp
    private/shader/glsl/resource/binding/global/GlslGlobalShaderResourceBinding.cpp
    private/shader/glsl/resource/binding/global/GlslGlobalShaderResourceBinding.h
    private/shader/glsl/DescriptorSetLayoutGenerator.h
    private/shader/glsl/DescriptorSetLayoutGenerator.cpp
    private/shader/glsl/GlslComputeShaderInterface.cpp
    private/shader/glsl/GlslComputeShaderInterface.h
    private/shader/general/resource/LightingShaderResourceManager.h
    private/shader/general/resource/LightingShaderResourceManager.cpp
    private/shader/general/resource/ShaderLightArray.h
    private/shader/general/resource/ShaderLightArray.cpp
    public/shader/ComputeShaderInterface.h
    private/shader/general/ComputeShaderInterface.cpp
    private/material/TextureManager.cpp
    public/material/TextureManager.h
    public/material/TextureFilteringPreference.h
    private/material/TextureFilteringPreference.cpp
    private/material/TextureHandle.cpp
    public/material/TextureHandle.h
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
    public/misc/Profiler.hpp
    private/misc/shapes/AABB.cpp
    public/misc/shapes/AABB.h
    private/misc/shapes/Frustum.cpp
    public/misc/shapes/Frustum.h
    private/misc/shapes/Plane.cpp
    public/misc/shapes/Plane.h
    private/misc/shapes/Sphere.cpp
    public/misc/shapes/Sphere.h
    private/misc/shapes/Cone.cpp
    public/misc/shapes/Cone.h
    private/shader/general/ShaderPack.h
    private/shader/general/ShaderPack.cpp
    private/shader/general/ShaderMacro.h
    private/shader/general/ShaderMacro.cpp
    private/shader/general/ShaderFilesystemPaths.hpp
    private/shader/general/resource/binding/cpuwrite/ShaderCpuWriteResourceBindingManager.cpp
    private/shader/general/resource/binding/cpuwrite/ShaderCpuWriteResourceBindingManager.h
    private/shader/general/resource/binding/cpuwrite/ShaderCpuWriteResourceBindingUniquePtr.cpp
    private/shader/general/resource/binding/cpuwrite/ShaderCpuWriteResourceBindingUniquePtr.h
    private/shader/general/resource/binding/texture/ShaderTextureResourceBindingManager.cpp
    private/shader/general/resource/binding/texture/ShaderTextureResourceBindingManager.h
    private/shader/general/resource/binding/texture/ShaderTextureResourceBindingUniquePtr.cpp
    private/shader/general/resource/binding/texture/ShaderTextureResourceBindingUniquePtr.h
    public/shader/VulkanAlignmentConstants.hpp
    public/misc/ProjectPaths.h
    private/misc/ProjectPaths.cpp
    public/misc/PrimitiveMeshGenerator.h
    private/misc/PrimitiveMeshGenerator.cpp
    private/io/Serializable.cpp
    public/io/Serializable.h
    public/game/camera/CameraProperties.h
    private/game/camera/CameraProperties.cpp
    public/game/camera/CameraManager.h
    private/game/camera/CameraManager.cpp
    public/game/node/callback/NodeFunction.hpp
    public/game/node/callback/NodeNotificationBroadcaster.hpp
    private/game/node/Node.cpp
    public/game/node/Node.h
    private/game/node/SpatialNode.cpp
    public/game/node/SpatialNode.h
    private/game/node/MeshNode.cpp
    public/game/node/MeshNode.h
    private/game/node/CameraNode.cpp
    public/game/node/CameraNode.h
    private/game/node/EnvironmentNode.cpp
    public/game/node/EnvironmentNode.h
    private/game/node/light/PointLightNode.cpp
    public/game/node/light/PointLightNode.h
    private/game/node/light/DirectionalLightNode.cpp
    public/game/node/light/DirectionalLightNode.h
    private/game/node/light/SpotlightNode.cpp
    public/game/node/light/SpotlightNode.h
    public/io/properties/SerializeProperty.h
    private/io/properties/SerializeProperty.cpp
    private/io/properties/GuidProperty.cpp
    public/io/properties/GuidProperty.h
    private/game/World.h
    private/game/World.cpp
    public/io/serializers/IFieldSerializer.hpp
    public/io/serializers/IBinaryFieldSerializer.hpp
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
    private/io/serializers/GlmVecFieldSerializer.cpp
    public/io/serializers/GlmVecFieldSerializer.h
    private/io/serializers/MeshDataBinaryFieldSerializer.cpp
    public/io/serializers/MeshDataBinaryFieldSerializer.h
    public/math/MathHelpers.hpp
    public/math/GLMath.hpp
    # add general (cross-platform) sources here...
)

# Append Windows specific sources if we are on Windows.
if (WIN32)
    list(APPEND PROJECT_SOURCES ${PROJECT_WIN32_SOURCES})
endif()

# Define target.
add_library(${PROJECT_NAME} STATIC ${PROJECT_SOURCES})

# -------------------------------------------------------------------------------------------------
#                                         CONFIGURE TARGET
# -------------------------------------------------------------------------------------------------

# Set target folder.
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ${ENGINE_FOLDER})

# Enable more warnings and warnings as errors.
enable_more_warnings()

# Set C++ standard.
set(PROJECT_CXX_STANDARD_VERSION 23)
set(CMAKE_CXX_STANDARD ${PROJECT_CXX_STANDARD_VERSION})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_${PROJECT_CXX_STANDARD_VERSION})
message(STATUS "${PROJECT_NAME}: using the following C++ standard: ${CMAKE_CXX_STANDARD}")

# Add includes.
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${RELATIVE_EXT_PATH})
target_include_directories(${PROJECT_NAME} PUBLIC public)
target_include_directories(${PROJECT_NAME} PUBLIC private)

# -------------------------------------------------------------------------------------------------
#                                           TOOLS
# -------------------------------------------------------------------------------------------------

# Enable Address Sanitizer in `Debug` builds on non-Windows OS.
if(NOT IS_RELEASE_BUILD AND NOT WIN32)
    enable_address_sanitizer()
endif()

# Enable Clang-tidy in `Release` builds.
if (IS_RELEASE_BUILD)
    enable_clang_tidy(${CMAKE_CURRENT_LIST_DIR}/../../.clang-tidy)
endif()

# -------------------------------------------------------------------------------------------------
#                                       DEPENDENCIES
# -------------------------------------------------------------------------------------------------

# External: glfw.
message(STATUS "${PROJECT_NAME}: adding external dependency \"GLFW\"...")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${RELATIVE_EXT_PATH}/glfw ${DEPENDENCY_BUILD_DIR_NAME}/glfw SYSTEM)
target_link_libraries(${PROJECT_NAME} PUBLIC glfw)
set_target_properties(glfw PROPERTIES FOLDER ${EXTERNAL_FOLDER})
set_target_properties(update_mappings PROPERTIES FOLDER ${EXTERNAL_FOLDER})

# External: spdlog.
message(STATUS "${PROJECT_NAME}: adding external dependency \"SPDLOG\"...")
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
set(SPDLOG_USE_STD_FORMAT ON CACHE BOOL "" FORCE)
add_subdirectory(${RELATIVE_EXT_PATH}/spdlog ${DEPENDENCY_BUILD_DIR_NAME}/spdlog SYSTEM)
target_link_libraries(${PROJECT_NAME} PUBLIC spdlog::spdlog)
set_target_properties(spdlog PROPERTIES FOLDER ${EXTERNAL_FOLDER})

# External: toml11.
message(STATUS "${PROJECT_NAME}: adding external dependency \"TOML11\"...")
if (MSVC)
    target_compile_options(${PROJECT_NAME} PUBLIC /Zc:__cplusplus)
endif()
add_subdirectory(${RELATIVE_EXT_PATH}/toml11 ${DEPENDENCY_BUILD_DIR_NAME}/toml11 SYSTEM)
set_target_properties(toml11 PROPERTIES FOLDER ${EXTERNAL_FOLDER})

# External: portable-file-dialogs
message(STATUS "${PROJECT_NAME}: adding external dependency \"portable-file-dialogs\"...")
add_subdirectory(${RELATIVE_EXT_PATH}/portable-file-dialogs ${DEPENDENCY_BUILD_DIR_NAME}/portable-file-dialogs SYSTEM)
target_link_libraries(${PROJECT_NAME} PUBLIC portable_file_dialogs)
set_target_properties(portable_file_dialogs PROPERTIES FOLDER ${EXTERNAL_FOLDER})

# External: GLM
message(STATUS "${PROJECT_NAME}: adding external dependency \"GLM\"...")
add_subdirectory(${RELATIVE_EXT_PATH}/glm ${DEPENDENCY_BUILD_DIR_NAME}/glm SYSTEM)
target_link_libraries(${PROJECT_NAME} PUBLIC glm)
set_target_properties(glm PROPERTIES FOLDER ${EXTERNAL_FOLDER})

# Add Vulkan SDK.
message(STATUS "${PROJECT_NAME}: adding external dependency \"Vulkan SDK\"...")
find_package(Vulkan REQUIRED) # check if vulkan sdk is installed
include(${RELATIVE_EXT_PATH}/Vulkan-Tools/cmake/FindVulkan.cmake) # include FindVulkan cmake helper to fill include/library paths
if (NOT Vulkan_FOUND)
    message(FATAL_ERROR "Failed to find Vulkan SDK. Please install Vulkan SDK that defines VULKAN_SDK environment variable.")
endif()
message(STATUS "${PROJECT_NAME}: found Vulkan include dirs at: ${Vulkan_INCLUDE_DIRS}")
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${Vulkan_LIBRARIES})

# External: combined-shader-language-parser
message(STATUS "${PROJECT_NAME}: adding external dependency \"Combined Shader Language Parser\"...")
set(CSL_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(CSL_ENABLE_DOXYGEN OFF CACHE BOOL "" FORCE)
add_subdirectory(${RELATIVE_EXT_PATH}/combined-shader-language-parser ${DEPENDENCY_BUILD_DIR_NAME}/combined-shader-language-parser SYSTEM)
target_link_libraries(${PROJECT_NAME} PUBLIC CombinedShaderLanguageParserLib)
set_target_properties(CombinedShaderLanguageParserLib PROPERTIES FOLDER ${EXTERNAL_FOLDER})

# Add "shaderc".
message(STATUS "${PROJECT_NAME}: adding external dependency \"shaderc\"...")
# Execute `shaderc` setup repo script
message(STATUS "${PROJECT_NAME}: running `git-sync-deps` of `shaderc`, please wait it may take a while...")
execute_process(COMMAND "python" "${ABSOLUTE_EXT_PATH}/shaderc/utils/git-sync-deps")
set(SHADERC_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SHADERC_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SHADERC_SKIP_COPYRIGHT_CHECK ON CACHE BOOL "" FORCE)
set(SHADERC_SKIP_TESTS ON CACHE BOOL "" FORCE)
set(SHADERC_SKIP_INSTALL ON CACHE BOOL "" FORCE)
set(SHADERC_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
set(SHADERC_ENABLE_WERROR_COMPILE ON CACHE BOOL "" FORCE)
set(SPIRV_HEADERS_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
set(SPIRV_HEADERS_SKIP_INSTALL ON CACHE BOOL "" FORCE)
set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
set(SPIRV_TOOLS_INSTALL_EMACS_HELPERS OFF CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_SPIRV_TOOLS_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${RELATIVE_EXT_PATH}/shaderc ${DEPENDENCY_BUILD_DIR_NAME}/shaderc SYSTEM)
target_link_libraries(${PROJECT_NAME} PUBLIC shaderc)
set_target_properties(shaderc PROPERTIES FOLDER ${EXTERNAL_FOLDER})

# External: SPIRV-Reflect
message(STATUS "${PROJECT_NAME}: adding external dependency \"SPIRV-Reflect\"...")
set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_STRIPPER OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_ENABLE_ASSERTS OFF CACHE BOOL "" FORCE)
set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "" FORCE)
add_subdirectory(${RELATIVE_EXT_PATH}/SPIRV-Reflect ${DEPENDENCY_BUILD_DIR_NAME}/SPIRV-Reflect SYSTEM)
target_link_libraries(${PROJECT_NAME} PUBLIC spirv-reflect-static)
set_target_properties(spirv-reflect-static PROPERTIES FOLDER ${EXTERNAL_FOLDER})

# External: KTX-Software
message(STATUS "${PROJECT_NAME}: adding external dependency \"KTX-Software\"...")
set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_LOADTEST_APPS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_GL_UPLOAD OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_DOC OFF CACHE BOOL "" FORCE)
add_subdirectory(${RELATIVE_EXT_PATH}/KTX-Software ${DEPENDENCY_BUILD_DIR_NAME}/KTX-Software SYSTEM)
target_link_libraries(${PROJECT_NAME} PUBLIC ktx)
set_target_properties(ktx PROPERTIES FOLDER ${EXTERNAL_FOLDER})

if(NOT IS_RELEASE_BUILD AND ENABLE_PROFILER)
    # External: Remotery
    message(STATUS "${PROJECT_NAME}: adding external dependency \"Remotery\"...")
    set(REMOTERY_TARGET_NAME Remotery)
    add_library(${REMOTERY_TARGET_NAME} STATIC
        ${ABSOLUTE_EXT_PATH}/Remotery/lib/Remotery.c
        ${ABSOLUTE_EXT_PATH}/Remotery/lib/Remotery.h)
    set_target_properties(${REMOTERY_TARGET_NAME} PROPERTIES FOLDER ${EXTERNAL_FOLDER})
    add_dependencies(${PROJECT_NAME} ${REMOTERY_TARGET_NAME})
    target_link_libraries(${PROJECT_NAME} PUBLIC ${REMOTERY_TARGET_NAME})
endif()

# External: Compressonator
message(STATUS "${PROJECT_NAME}: adding external dependency \"Compressonator\"...")
# Execute `Compressonator` setup repo script
message(STATUS "${PROJECT_NAME}: fetching \"Compressonator\" dependencies, please wait it may take a while...")
execute_process(
    COMMAND "python" "${ABSOLUTE_EXT_PATH}/compressonator/build/fetch_dependencies.py"
    WORKING_DIRECTORY "${ABSOLUTE_EXT_PATH}/compressonator/build"
)
add_subdirectory(${RELATIVE_EXT_PATH}/compressonator/cmp_compressonatorlib ${DEPENDENCY_BUILD_DIR_NAME}/cmp_compressonatorlib SYSTEM)
target_link_libraries(${PROJECT_NAME} PUBLIC CMP_Compressonator)
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${RELATIVE_EXT_PATH}/compressonator/cmp_compressonatorlib)
set_target_properties(CMP_Compressonator PROPERTIES FOLDER ${EXTERNAL_FOLDER})
add_subdirectory(${RELATIVE_EXT_PATH}/compressonator/cmp_core ${DEPENDENCY_BUILD_DIR_NAME}/cmp_core SYSTEM)
target_link_libraries(${PROJECT_NAME} PUBLIC CMP_Core)
set_target_properties(CMP_Core PROPERTIES FOLDER ${EXTERNAL_FOLDER})
add_subdirectory(${RELATIVE_EXT_PATH}/compressonator/cmp_framework ${DEPENDENCY_BUILD_DIR_NAME}/cmp_framework SYSTEM)
target_link_libraries(${PROJECT_NAME} PUBLIC CMP_Framework)
set_target_properties(CMP_Framework PROPERTIES FOLDER ${EXTERNAL_FOLDER})

# External: xxHash
message(STATUS "${PROJECT_NAME}: adding external dependency \"xxHash\"...")
set(XXHASH_TARGET_NAME xxHash)
add_library(${XXHASH_TARGET_NAME} STATIC
    ${RELATIVE_EXT_PATH}/xxHash/xxhash.c
    ${RELATIVE_EXT_PATH}/xxHash/xxhash.h)
set_target_properties(${XXHASH_TARGET_NAME} PROPERTIES FOLDER ${EXTERNAL_FOLDER})
add_dependencies(${PROJECT_NAME} ${XXHASH_TARGET_NAME})
target_link_libraries(${PROJECT_NAME} PUBLIC ${XXHASH_TARGET_NAME})

# External: tinygltf
message(STATUS "${PROJECT_NAME}: adding external dependency \"tinygltf\"...")
set(TINYGLTF_HEADER_ONLY ON CACHE BOOL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
add_subdirectory(${RELATIVE_EXT_PATH}/tinygltf ${DEPENDENCY_BUILD_DIR_NAME}/tinygltf SYSTEM)
target_link_libraries(${PROJECT_NAME} PUBLIC tinygltf)
set_target_properties(tinygltf PROPERTIES FOLDER ${EXTERNAL_FOLDER})
add_dependencies(${PROJECT_NAME} tinygltf)

# External: sgc
message(STATUS "${PROJECT_NAME}: adding external dependency \"sgc\"...")
set(SGC_GENERATE_DOCS OFF CACHE BOOL "" FORCE)
set(SGC_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${RELATIVE_EXT_PATH}/sgc ${DEPENDENCY_BUILD_DIR_NAME}/sgc SYSTEM)
target_link_libraries(${PROJECT_NAME} PUBLIC sgc_lib)
set_target_properties(${SGC_TARGET_NAME} PROPERTIES FOLDER ${EXTERNAL_FOLDER})
add_dependencies(${PROJECT_NAME} sgc_lib)

# External: Refureku
set(REFUREKU_EXCLUDED_FILES "")
if (NOT WIN32)
    foreach(RELATIVE_PATH IN LISTS PROJECT_WIN32_SOURCES)
        set(REFUREKU_EXCLUDED_FILES ${REFUREKU_EXCLUDED_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/${RELATIVE_PATH})
    endforeach()
endif()
# Write project include directories for Refureku.
set(REFUREKU_INCLUDE_DIRECTORIES
    ${ABSOLUTE_EXT_PATH}
    ${ABSOLUTE_EXT_PATH}/glfw/include
    ${ABSOLUTE_EXT_PATH}/DirectX-Headers/include
    ${ABSOLUTE_EXT_PATH}/spdlog/include
    ${ABSOLUTE_EXT_PATH}/glm
    ${ABSOLUTE_EXT_PATH}/sgc/src/sgc_lib/private
    ${ABSOLUTE_EXT_PATH}/sgc/src/sgc_lib/public
    ${ABSOLUTE_EXT_PATH}/Refureku/build/Include
    ${ABSOLUTE_EXT_PATH}/combined-shader-language-parser/src/csl_lib/src
    ${ABSOLUTE_EXT_PATH}/shaderc/libshaderc/include
    ${ABSOLUTE_EXT_PATH}/SPIRV-Reflect
    ${ABSOLUTE_EXT_PATH}/Remotery/lib
    ${ABSOLUTE_EXT_PATH}/KTX-Software/include
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/public
    ${CMAKE_CURRENT_SOURCE_DIR}/private
)
add_refureku(
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${REFUREKU_EXCLUDED_FILES}"
    "${ABSOLUTE_EXT_PATH}/Refureku"
    ""
    "${REFUREKU_INCLUDE_DIRECTORIES}"
)

# Win32 specific dependencies:
if (WIN32)
    # External: D3D12MemoryAllocator
    message(STATUS "${PROJECT_NAME}: adding external dependency \"D3D12MemoryAllocator\"...")
    add_subdirectory(${RELATIVE_EXT_PATH}/D3D12MemoryAllocator ${DEPENDENCY_BUILD_DIR_NAME}/D3D12MemoryAllocator SYSTEM)
    target_link_libraries(${PROJECT_NAME} PUBLIC D3D12MemoryAllocator)
    set_target_properties(D3D12MemoryAllocator PROPERTIES FOLDER ${EXTERNAL_FOLDER})
    if(NOT MSVC)
        set_source_files_properties(${RELATIVE_EXT_PATH}/D3D12MemoryAllocator/src/D3D12MemAlloc.cpp PROPERTIES COMPILE_FLAGS "-Wno-tautological-undefined-compare")
        set_source_files_properties(${RELATIVE_EXT_PATH}/D3D12MemoryAllocator/src/D3D12MemAlloc.cpp PROPERTIES COMPILE_FLAGS "-Wno-switch")
    endif()

    # External: DirectX-Headers
    message(STATUS "${PROJECT_NAME}: adding external dependency \"DirectX-Headers\"...")
    add_subdirectory(${RELATIVE_EXT_PATH}/DirectX-Headers ${DEPENDENCY_BUILD_DIR_NAME}/DirectX-Headers SYSTEM)
    target_link_libraries(${PROJECT_NAME} PUBLIC DirectX-Headers)
    set_target_properties(DirectX-Headers PROPERTIES FOLDER ${EXTERNAL_FOLDER})
    set_target_properties(DirectX-Guids PROPERTIES FOLDER ${EXTERNAL_FOLDER})
    if(NOT MSVC)
        set_source_files_properties(${RELATIVE_EXT_PATH}/DirectX-Headers/src/d3dx12_property_format_table.cpp PROPERTIES COMPILE_FLAGS "-Wno-switch")
    endif()

    # External: DXC.
    # Download and unzip DXC.
    message(STATUS "${PROJECT_NAME}: adding external dependency \"DirectXShaderCompiler\"...")
    message(STATUS "${PROJECT_NAME}: downloading DirectXShaderCompiler...")
    execute_process(
        COMMAND go run ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/DirectXShaderCompiler/download_dxc.go
        ${ABSOLUTE_EXT_PATH}/DirectXShaderCompiler/ # working directory
        OUTPUT_VARIABLE dxc_output
        ERROR_VARIABLE dxc_error
    )
    message(STATUS "${PROJECT_NAME}: ${dxc_output}")
    if (dxc_error)
        message(FATAL_ERROR "${dxc_error}")
    endif()
    # Set DXC variables.
    set(PATH_TO_DXC_DLL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/DirectXShaderCompiler/bin/x64)
    set(PATH_TO_DXC_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../ext/DirectXShaderCompiler/lib/x64)
    # Link to DXC libraries.
    target_link_directories(${PROJECT_NAME} PUBLIC ${PATH_TO_DXC_DLL_DIR})
    target_link_directories(${PROJECT_NAME} PUBLIC ${PATH_TO_DXC_LIB_DIR})
    target_link_libraries(${PROJECT_NAME} PUBLIC ${PATH_TO_DXC_LIB_DIR}/dxcompiler.lib)

    # External: DDSTextureLoader
    message(STATUS "${PROJECT_NAME}: adding external dependency \"DDSTextureLoader\"...")
    set(DDSTEXTURELOADER_TARGET_NAME DDSTextureLoader)
    add_library(${DDSTEXTURELOADER_TARGET_NAME} STATIC
        ${RELATIVE_EXT_PATH}/DirectXTex/DDSTextureLoader/DDSTextureLoader12.h
        ${RELATIVE_EXT_PATH}/DirectXTex/DDSTextureLoader/DDSTextureLoader12.cpp)
    target_include_directories(${DDSTEXTURELOADER_TARGET_NAME} PRIVATE ${ABSOLUTE_EXT_PATH}/DirectX-Headers/include/directx)
    set_target_properties(${DDSTEXTURELOADER_TARGET_NAME} PROPERTIES FOLDER ${EXTERNAL_FOLDER})
    add_dependencies(${PROJECT_NAME} ${DDSTEXTURELOADER_TARGET_NAME})
    target_link_libraries(${PROJECT_NAME} PUBLIC ${DDSTEXTURELOADER_TARGET_NAME})
endif()
