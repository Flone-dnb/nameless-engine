#pragma once

// Standard.
#include <optional>

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
         * Creates a new constant buffer view descriptor that points to this resource.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindCbv() = 0;
    };
} // namespace ne
