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
        /** Types of descriptors that point to resources and specify how a resource should be used. */
        enum class DescriptorType : size_t {
            RTV = 0,
            DSV,
            CBV,
            SRV,
            UAV,
            END // marks the end of this enum
        };

        GpuResource() = default;
        virtual ~GpuResource() = default;

        GpuResource(GpuResource&) = delete;
        GpuResource& operator=(GpuResource&) = delete;

        /**
         * Creates a new descriptor and binds it to this resource.
         *
         * @param descriptorType Type of descriptor to bind.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindDescriptor(DescriptorType descriptorType) = 0;

        /**
         * Returns resource name.
         *
         * @return Resource name.
         */
        virtual std::string getResourceName() const = 0;
    };
} // namespace ne
