﻿#pragma once

// Standard.
#include <variant>
#include <vector>
#include <unordered_map>
#include <array>
#include <unordered_set>

// Custom.
#include "misc/Error.h"
#include "render/general/resource/frame/FrameResourceManager.h"
#include "shader/VulkanAlignmentConstants.hpp"
#include "misc/StdHashes.hpp"

// External.
#include "vulkan/vulkan.h"

namespace ne {
    class GlslShader;
    class VulkanRenderer;

    /** Describes type of a resource that was written in the GLSL code. */
    enum class GlslResourceType {
        UNIFORM_BUFFER,
        STORAGE_BUFFER,
        COMBINED_SAMPLER,
        STORAGE_IMAGE,
        // ... add new resource types here ...
    };

    /** Generates Descriptor Set Layout based on GLSL code. */
    class DescriptorSetLayoutGenerator {
    public:
        /** Groups collected info. */
        struct Collected {
            /** Contains information about a descriptor set layout binding. */
            struct DescriptorSetLayoutBindingInfo {
                /** Type of the shader resource. */
                GlslResourceType resourceType;

                /** Name of the resource (written in the GLSL code). */
                std::string sResourceName;
            };

            /**
             * Map of descriptor set layout binding where key is binding index and value
             * is descriptor info.
             */
            std::unordered_map<uint32_t, DescriptorSetLayoutBindingInfo> bindingInfo;

            /**
             * Stores pairs of "name of field defined in GLSL push constants" (all with `uint` type)
             * and "offset from the beginning of the push constants struct (in `uint`s not bytes)".
             *
             * @remark May be empty if not used.
             *
             * @remark If a non `uint` fields is found an error is returned instead.
             */
            std::unordered_map<std::string, size_t> pushConstantUintFieldOffsets;
        };

        /** Groups generated data. */
        struct Generated {
            /** Created descriptor set layout. */
            VkDescriptorSetLayout pDescriptorSetLayout = nullptr;

            /** Created descriptor pool. */
            VkDescriptorPool pDescriptorPool = nullptr;

            /** Created descriptor set per each frame resource. */
            std::array<VkDescriptorSet, FrameResourceManager::getFrameResourceCount()> vDescriptorSets;

            /**
             * Map of pairs "resource name" (from GLSL code) - "layout binding index".
             *
             * @remark Binding index in the map reference descriptor sets from @ref vDescriptorSets.
             *
             * @remark Generally used to bind/update data of some GLSL resource to a specific
             * descriptor in a descriptor set.
             */
            std::unordered_map<std::string, uint32_t, StdStringHash, std::equal_to<>> resourceBindings;

            /**
             * Stores pairs of "name of field defined in GLSL push constants" (all with `uint` type)
             * and "offset from the beginning of the push constants struct (in `uint`s not bytes)".
             *
             * @remark May be empty if not used.
             *
             * @remark If a non `uint` fields is found an error is returned instead.
             */
            std::unordered_map<std::string, size_t> pushConstantUintFieldOffsets;
        };

        DescriptorSetLayoutGenerator() = delete;
        DescriptorSetLayoutGenerator(const DescriptorSetLayoutGenerator&) = delete;
        DescriptorSetLayoutGenerator& operator=(const DescriptorSetLayoutGenerator&) = delete;

        /**
         * Collection information from the specified SPIR-V bytecode that can be used to generate
         * a descriptor set layout.
         *
         * @param pSpirvBytecode SPIR-V bytecode to analyze.
         * @param iSizeInBytes   Size of bytecode in bytes.
         *
         * @return Error if something went wrong, otherwise collected info.
         */
        static std::variant<Collected, Error>
        collectInfoFromBytecode(void* pSpirvBytecode, size_t iSizeInBytes);

        /**
         * Generates a new descriptor layout, pool and descriptor sets using the specified vertex and
         * fragment shaders.
         *
         * @remark Expects that descriptor layout information is already collected for both
         * shaders (see @ref collectInfoFromBytecode), otherwise returns error.
         *
         * @param pRenderer       Vulkan renderer.
         * @param pVertexShader   Vertex shader.
         * @param pFragmentShader Fragment shader. Specify `nullptr` to generate descriptor layout only for
         * vertex shader.
         *
         * @return Error if something went wrong, otherwise generated descriptor layout data.
         */
        static std::variant<Generated, Error>
        generateGraphics(VulkanRenderer* pRenderer, GlslShader* pVertexShader, GlslShader* pFragmentShader);

        /**
         * Generates a new descriptor layout, pool and descriptor sets using the specified compute shader.
         *
         * @remark Expects that descriptor layout information is already collected for both
         * shaders (see @ref collectInfoFromBytecode), otherwise returns error.
         *
         * @param pRenderer       Vulkan renderer.
         * @param pComputeShader  Compute shader.
         *
         * @return Error if something went wrong, otherwise generated descriptor layout data.
         */
        static std::variant<Generated, Error>
        generateCompute(VulkanRenderer* pRenderer, GlslShader* pComputeShader);

    private:
        /**
         * Generates Vulkan layout binding that could be used to create a descriptor set layout.
         *
         * @param iBindingIndex    Index of the binding that was specified in the GLSL code.
         * @param bindingInfo      Information about the GLSL resource used in this binding.
         * @param bIsComputeShader `true` if this binding is being generated for a compute shader, otherwise
         * `false`.
         *
         * @return Error if something went wrong, otherwise generated binding with flags.
         */
        static std::variant<std::pair<VkDescriptorSetLayoutBinding, VkDescriptorBindingFlags>, Error>
        generateLayoutBinding(
            uint32_t iBindingIndex,
            const Collected::DescriptorSetLayoutBindingInfo& bindingInfo,
            bool bIsComputeShader);
    };
} // namespace ne
