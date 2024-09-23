#pragma once

// Standrad.
#include <mutex>
#include <unordered_map>
#include <string>
#include <memory>
#include <variant>
#include <optional>

// Custom.
#include "misc/Error.h"

namespace ne {
    class DynamicCpuWriteShaderResourceArray;
    class DynamicCpuWriteShaderResourceArraySlot;
    class ShaderCpuWriteResource;
    class VulkanRenderer;
    class VulkanPipeline;

    /**
     * Manages CPU-write arrays to shader resource arrays.
     *
     * Its main purpose is to avoid possible duplication of shader resource arrays (when 2 or more arrays
     * handle the same shader resource).
     */
    class DynamicCpuWriteShaderResourceArrayManager {
        // Only resource manager is supposed to own this manager.
        friend class GpuResourceManager;

    public:
        DynamicCpuWriteShaderResourceArrayManager() = delete;

        ~DynamicCpuWriteShaderResourceArrayManager();

        DynamicCpuWriteShaderResourceArrayManager(const DynamicCpuWriteShaderResourceArrayManager&) = delete;
        DynamicCpuWriteShaderResourceArrayManager&
        operator=(const DynamicCpuWriteShaderResourceArrayManager&) = delete;

        /**
         * Requests a new slot in the array to be reserved for use by the specified shader resource.
         *
         * @remark There is no `erase` function because slot destruction automatically
         * uses internal `erase`, see documentation on the returned slot object.
         *
         * @param pShaderResource Shader resource that requires a slot in the array.
         * If the array will resize, the specified resource (if it has an active slot in the array)
         * will be automatically marked as "needs update" through the shader resource manager.
         *
         * @return Error if something went wrong, otherwise slot of the newly added element in
         * the array.
         */
        std::variant<std::unique_ptr<DynamicCpuWriteShaderResourceArraySlot>, Error>
        reserveSlotsInArray(ShaderCpuWriteResource* pShaderResource);

        /**
         * Attempts to find an array that handles shader resource of the specified name.
         *
         * @remark Mostly used for automated testing.
         *
         * @param sShaderResourceName Name of the shader resource (from shader code).
         *
         * @return `nullptr` if not found, otherwise a valid pointer.
         */
        DynamicCpuWriteShaderResourceArray* getArrayForShaderResource(const std::string& sShaderResourceName);

    private:
        /**
         * Formats the specified size in bytes to the following format: "<number> MB",
         * for example the number 1512 will be formatted to the following text: "0.0014 MB".
         *
         * @param iSizeInBytes Size in bytes to format.
         *
         * @return Formatted text.
         */
        static std::string formatBytesToMegabytes(size_t iSizeInBytes);

        /**
         * Resource manager that owns this manager
         *
         * @param pResourceManager Owner manager.
         */
        DynamicCpuWriteShaderResourceArrayManager(GpuResourceManager* pResourceManager);

        /** Goes through all arrays in @ref mtxCpuWriteShaderResourceArrays and removed empty ones. */
        void removeEmptyArrays();

        /** Owner of this manager. */
        GpuResourceManager* const pResourceManager = nullptr;

        /**
         * Stores pairs of "shader resource name" - "array that handles the shader resource",
         * where "shader resource name" is the name of the resource written in the shader file.
         */
        std::pair<
            std::recursive_mutex,
            std::unordered_map<std::string, std::unique_ptr<DynamicCpuWriteShaderResourceArray>>>
            mtxCpuWriteShaderResourceArrays;
    };
}
