#pragma once

// Standard.
#include <optional>
#include <string>

// Custom.
#include "misc/Error.h"

namespace ne {
    /** Resource stored in the GPU memory. */
    class GpuResource {
    public:
        GpuResource() = delete;

        /**
         * Initializes resource.
         *
         * @param sResourceName       Name of this resource.
         * @param iElementSizeInBytes Resource size information. Size of one array element (if array),
         * otherwise specify size of the whole resource.
         * @param iElementCount       Resource size information. Total number of elements in the array (if
         * array), otherwise specify 1.
         */
        GpuResource(
            const std::string& sResourceName, unsigned int iElementSizeInBytes, unsigned int iElementCount);

        virtual ~GpuResource() = default;

        GpuResource(GpuResource&) = delete;
        GpuResource& operator=(GpuResource&) = delete;

        /**
         * Returns resource name.
         *
         * @return Resource name.
         */
        std::string getResourceName() const;

        /**
         * Returns resource size information. Size of one array element (if array),
         * otherwise size of the whole resource.
         *
         * @remark May be zero in some cases.
         *
         * @return Size in bytes.
         */
        unsigned int getElementSizeInBytes() const;

        /**
         * Returns resource size information. Total number of elements in the array (if
         * array), otherwise 1.
         *
         * @remark May be zero in some cases.
         *
         * @return Size in bytes.
         */
        unsigned int getElementCount() const;

    private:
        /** Resource size information (may be zero in some cases). */
        const unsigned int iElementSizeInBytes = 0;

        /** Resource size information (may be zero in some cases). */
        const unsigned int iElementCount = 0;

        /** Name of this resource. */
        const std::string sResourceName;
    };
} // namespace ne
