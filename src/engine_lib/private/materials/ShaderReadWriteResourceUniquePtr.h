#pragma once

// Standard.
#include <mutex>

namespace ne {
    class Node;
    class ShaderCpuReadWriteResourceManager;
    class ShaderCpuReadWriteResource;

    /**
     * Small raw pointer wrapper that acts like `std::unique_ptr`
     * for ShaderCpuReadWriteResource to do some extra work
     * when started/stopped referencing a resource.
     *
     * When deleted causes the resource to be also deleted.
     */
    class ShaderCpuReadWriteResourceUniquePtr {
        // Only manager can create objects of this class.
        friend class ShaderCpuReadWriteResourceManager;

    public:
        ShaderCpuReadWriteResourceUniquePtr() = default;

        ShaderCpuReadWriteResourceUniquePtr(const ShaderCpuReadWriteResourceUniquePtr&) = delete;
        ShaderCpuReadWriteResourceUniquePtr& operator=(const ShaderCpuReadWriteResourceUniquePtr&) = delete;

        ~ShaderCpuReadWriteResourceUniquePtr();

        /**
         * Move constructor.
         *
         * @param other Other object.
         */
        ShaderCpuReadWriteResourceUniquePtr(ShaderCpuReadWriteResourceUniquePtr&& other) noexcept;

        /**
         * Move assignment.
         *
         * @param other Other object.
         *
         * @return Result of move assignment.
         */
        ShaderCpuReadWriteResourceUniquePtr& operator=(ShaderCpuReadWriteResourceUniquePtr&& other) noexcept;

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
        inline ShaderCpuReadWriteResource* getResource() const { return pResource; }

    private:
        /**
         *
         * @param pManager  Manager that owns the resource.
         * @param pResource Resource to point to.
         */
        ShaderCpuReadWriteResourceUniquePtr(
            ShaderCpuReadWriteResourceManager* pManager, ShaderCpuReadWriteResource* pResource);

        /** Manager that owns the resource we are pointing to. */
        ShaderCpuReadWriteResourceManager* pManager = nullptr;

        /** Resource we are pointing to. */
        ShaderCpuReadWriteResource* pResource = nullptr;
    };
} // namespace ne
