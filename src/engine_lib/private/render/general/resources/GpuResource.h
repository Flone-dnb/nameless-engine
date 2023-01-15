#pragma once

namespace ne {
    /** Resource stored in the GPU memory. */
    class GpuResource {
    public:
        GpuResource() = default;
        virtual ~GpuResource() = default;

        GpuResource(GpuResource&) = delete;
        GpuResource& operator=(GpuResource&) = delete;
    };
} // namespace ne
