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
         * Creates a new render target view descriptor that points to this resource.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindRtv() = 0;

        /**
         * Creates a new depth stencil view descriptor that points to this resource.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindDsv() = 0;

        /**
         * Creates a new constant buffer view descriptor that points to this resource.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindCbv() = 0;

        /**
         * Creates a new shader resource view descriptor that points to this resource.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindSrv() = 0;

        /**
         * Creates a new unordered access view descriptor that points to this resource.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindUav() = 0;
    };
} // namespace ne
