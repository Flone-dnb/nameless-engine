#pragma once

namespace ne {
    /** Defines some descriptor-related constants. */
    struct DescriptorConstants {
        DescriptorConstants() = delete;

        /**
         * Defines the maximum amount of descriptors a bindless texture array/table
         * will have.
         *
         * @remark With bindless arrays/tables we generally think that there's no limit but
         * when creating them we need to provide some value as array size so let's use this value.
         */
        static constexpr unsigned int iBindlessTextureArrayDescriptorCount = 20000; // NOLINT: magic number
    };
}
