#pragma once

// Standard.
#include <stdexcept>
#include <format>
#include <vector>

// Custom.
#include "misc/Error.h"
#include "io/Logger.h"

namespace ne {
    /** Stores data to copy to push constants (Vulkan) or root constants (DirectX). */
    class PipelineShaderConstantsManager {
    public:
        /** Type of the variables the manager stores. */
        using variable_type = unsigned int;

        PipelineShaderConstantsManager() = delete;
        PipelineShaderConstantsManager(const PipelineShaderConstantsManager&) = delete;
        PipelineShaderConstantsManager& operator=(const PipelineShaderConstantsManager&) = delete;

        /** Groups names of special (some non-user specified) push/root constants. */
        struct SpecialConstantsNames {
            /** Index into array of viewProjection matrices of spawned light sources (for shadow mapping). */
            static constexpr auto pLightViewProjectionMatrixIndex = "iLightViewProjectionMatrixIndex";
        };

        /**
         * Returns the maximum size of constants data that the manager allows to have.
         *
         * @return Maximum size of constants in bytes.
         */
        static constexpr size_t getMaxConstantsSizeInBytes() { return iMaxShaderConstantsSizeInBytes; }

        /**
         * Creates a new manager that stores the specified number of `unsigned int` variables
         * as push/root constants.
         *
         * @warning Shows an error and throws an exception if the specified size (in bytes not in
         * `unsigned int`s) will exceed @ref getMaxConstantsSizeInBytes.
         *
         * @param iVariableCount Defines how much `unsigned int`s to store as push/root constants.
         */
        PipelineShaderConstantsManager(size_t iVariableCount) {
            // Make sure we won't exceed maximum allowed size limit.
            const size_t iSizeInBytes = iVariableCount * sizeof(variable_type);
            if (iSizeInBytes > iMaxShaderConstantsSizeInBytes) [[unlikely]] {
                Error error(std::format(
                    "failed to create shader constants manager with size {} bytes because the maximum "
                    "allowed size is {} bytes",
                    iSizeInBytes,
                    iMaxShaderConstantsSizeInBytes));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Make sure the specified size is not zero.
            if (iVariableCount == 0) [[unlikely]] {
                Error error("failed to create shader constants manager because the specified size is zero");
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Prepare data.
            vConstantsData.resize(iVariableCount, 0);
        }

        /**
         * Copies the specified value into the specified push/root constant.
         *
         * @param iShaderConstantIndex Index of the push/root constant to copy the data to.
         * @param iValueToCopy         Value to copy.
         */
        inline void copyValueToShaderConstant(size_t iShaderConstantIndex, unsigned int iValueToCopy) {
            // Make sure we don't access out of bounds.
            if (iShaderConstantIndex >= vConstantsData.size()) [[unlikely]] {
                Logger::get().error(std::format(
                    "the specified push/root constant index {} is out of bounds (max: {})",
                    iShaderConstantIndex,
                    vConstantsData.size() - 1));
                return;
            }

            // Set value.
            vConstantsData[iShaderConstantIndex] = iValueToCopy;
        }

        /**
         * Returns total size of push/root constants data in bytes.
         *
         * @return Size in bytes. Returning `unsigned int` since Vulkan and DirectX operate on `uint`s.
         */
        inline unsigned int getTotalSizeInBytes() const {
            // We can safely cast to `uint` here since push/root constants are very small in size.
            return static_cast<unsigned int>(vConstantsData.size() * sizeof(variable_type));
        }

        /**
         * Returns the total number of `unsigned int`s stored as push/root constants.
         *
         * @return Number of variables.
         */
        inline unsigned int getVariableCount() const {
            return static_cast<unsigned int>(vConstantsData.size());
        }

        /**
         * Returns pointer to the beginning of the push/root constants data.
         *
         * @return Push/root constants data.
         */
        inline void* getData() { return vConstantsData.data(); }

    private:
        /** Data that will be copied to push/root constants. */
        std::vector<variable_type> vConstantsData;

        /** Defines the maximum size of constants in total (in bytes). */
        static constexpr size_t iMaxShaderConstantsSizeInBytes =
            128; // NOLINT: guaranteed minimum supported size from Vulkan specs, we should stick with it to
                 // support as much GPUs as possible
    };
} // namespace ne
