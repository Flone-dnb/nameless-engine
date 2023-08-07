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
        GpuResource() = default;
        virtual ~GpuResource() = default;

        GpuResource(GpuResource&) = delete;
        GpuResource& operator=(GpuResource&) = delete;

        /**
         * Returns resource name.
         *
         * @return Resource name.
         */
        virtual std::string getResourceName() const = 0;
    };
} // namespace ne
