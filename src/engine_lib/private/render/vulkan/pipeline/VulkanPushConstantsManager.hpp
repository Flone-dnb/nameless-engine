#pragma once

// Standard.
#include <stdexcept>
#include <format>
#include <vector>

// Custom.
#include "misc/Error.h"
#include "io/Logger.h"

namespace ne {
    /** Stores push constants. */
    class VulkanPushConstantsManager {
    public:
        VulkanPushConstantsManager() = delete;
        VulkanPushConstantsManager(const VulkanPushConstantsManager&) = delete;
        VulkanPushConstantsManager& operator=(const VulkanPushConstantsManager&) = delete;

        /**
         * Returns the maximum size of push constants data that the manager allows to have.
         *
         * @return Maximum size of push constants in bytes.
         */
        static constexpr size_t getMaxPushConstantsSizeInBytes() { return iMaxPushConstantsSizeInBytes; }

        /**
         * Creates a new manager that stores the specified number of `unsigned int` variables
         * as push constants.
         *
         * @warning Shows an error and throws an exception if the specified size (in bytes not in
         * `unsigned int`s) will exceed @ref getMaxPushConstantsSizeInBytes.
         *
         * @param iSize Defines how much `unsigned int`s to store as push constants.
         */
        VulkanPushConstantsManager(size_t iSize) {
            // Make sure we won't exceed maximum allowed size limit.
            const size_t iSizeInBytes = iSize * sizeof(unsigned int);
            if (iSizeInBytes > iMaxPushConstantsSizeInBytes) [[unlikely]] {
                Error error(std::format(
                    "failed to create push constants manager with size {} bytes because the maximum allowed "
                    "size is {} bytes",
                    iSizeInBytes,
                    iMaxPushConstantsSizeInBytes));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Make sure the specified size is not zero.
            if (iSize == 0) [[unlikely]] {
                Error error("failed to create push constants manager because the specified size is zero");
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Prepare data.
            vPushConstantsData.resize(iSize, 0);
        }

        /**
         * Copies the specified value into the specified push constant.
         *
         * @param iPushConstantIndex Index of the push constant to copy the data to.
         * @param iValueToCopy       Value to copy.
         */
        inline void copyValueToPushConstant(size_t iPushConstantIndex, unsigned int iValueToCopy) {
            // Make sure we don't access out of bounds.
            if (iPushConstantIndex >= vPushConstantsData.size()) [[unlikely]] {
                Logger::get().error(std::format(
                    "the specified push constant index {} is out of bounds (max: {})",
                    iPushConstantIndex,
                    vPushConstantsData.size() - 1));
                return;
            }

            // Set value.
            vPushConstantsData[iPushConstantIndex] = iValueToCopy;
        }

        /**
         * Returns total size of push constants data in bytes.
         *
         * @return Size in bytes. Returning `unsigned int` since Vulkan works with `uint32_t`s.
         */
        inline unsigned int getTotalSizeInBytes() const {
            // We can safely cast to `uint` here since push constants are very small.
            return static_cast<unsigned int>(vPushConstantsData.size() * sizeof(vPushConstantsData[0]));
        }

        /**
         * Returns pointer to the beginning of the push constants data.
         *
         * @return Push constants data.
         */
        inline void* getData() { return reinterpret_cast<void*>(vPushConstantsData.data()); }

    private:
        /** Data that will be copied to push constants: array of `uint`s. */
        std::vector<unsigned int> vPushConstantsData;

        /** Defines the maximum size of push constants in total (in bytes). */
        static constexpr size_t iMaxPushConstantsSizeInBytes =
            128; // NOLINT: guaranteed minimum supported size from Vulkan specs, we should stick with it in
                 // order to avoid game working on 1 GPU and not working on another, moreover this should be
                 // more than enough for our needs
    };
} // namespace ne
