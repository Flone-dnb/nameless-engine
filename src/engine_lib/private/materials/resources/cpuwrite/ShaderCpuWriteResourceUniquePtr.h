#pragma once

// Standard.
#include <mutex>

namespace ne {
    class ShaderCpuWriteResourceManager;
    class ShaderCpuWriteResource;

    /**
     * Small raw pointer wrapper that acts like `std::unique_ptr`
     * for shader CPU write resources to do some extra work
     * when started/stopped referencing a resource.
     *
     * When deleted causes the resource to be also deleted.
     */
    class ShaderCpuWriteResourceUniquePtr {
        // Only manager can create objects of this class.
        friend class ShaderCpuWriteResourceManager;

    public:
        ShaderCpuWriteResourceUniquePtr() = default;

        ShaderCpuWriteResourceUniquePtr(const ShaderCpuWriteResourceUniquePtr&) = delete;
        ShaderCpuWriteResourceUniquePtr& operator=(const ShaderCpuWriteResourceUniquePtr&) = delete;

        ~ShaderCpuWriteResourceUniquePtr();

        /**
         * Move constructor.
         *
         * @param other Other object.
         */
        ShaderCpuWriteResourceUniquePtr(ShaderCpuWriteResourceUniquePtr&& other) noexcept;

        /**
         * Move assignment.
         *
         * @param other Other object.
         *
         * @return Result of move assignment.
         */
        ShaderCpuWriteResourceUniquePtr& operator=(ShaderCpuWriteResourceUniquePtr&& other) noexcept;

        /**
         * Marks shader resource as "needs update", this will cause resource's update callback
         * to be called multiple times.
         */
        void markAsNeedsUpdate();

        /**
         * Returns the underlying resource.
         *
         * @return `nullptr` if moved or not initialized, otherwise valid pointer to resource.
         */
        inline ShaderCpuWriteResource* getResource() const { return pResource; }

    private:
        /**
         * Constructs a new unique pointer.
         *
         * @param pManager  Manager that owns the resource.
         * @param pResource Resource to point to.
         */
        ShaderCpuWriteResourceUniquePtr(
            ShaderCpuWriteResourceManager* pManager, ShaderCpuWriteResource* pResource);

        /** Manager that owns the resource we are pointing to. */
        ShaderCpuWriteResourceManager* pManager = nullptr;

        /** Resource we are pointing to. */
        ShaderCpuWriteResource* pResource = nullptr;
    };
} // namespace ne
