#pragma once

namespace ne {
    class ShaderCpuWriteResourceBindingManager;
    class ShaderCpuWriteResourceBinding;

    /**
     * Small raw pointer wrapper that acts like `std::unique_ptr`
     * for shader CPU write resource bindings to do some extra work
     * when started/stopped referencing a resource.
     *
     * When deleted causes the resource to be also deleted.
     */
    class ShaderCpuWriteResourceBindingUniquePtr {
        // Only manager can create objects of this class.
        friend class ShaderCpuWriteResourceBindingManager;

    public:
        ShaderCpuWriteResourceBindingUniquePtr() = default;

        ShaderCpuWriteResourceBindingUniquePtr(const ShaderCpuWriteResourceBindingUniquePtr&) = delete;
        ShaderCpuWriteResourceBindingUniquePtr&
        operator=(const ShaderCpuWriteResourceBindingUniquePtr&) = delete;

        ~ShaderCpuWriteResourceBindingUniquePtr();

        /**
         * Move constructor.
         *
         * @param other Other object.
         */
        ShaderCpuWriteResourceBindingUniquePtr(ShaderCpuWriteResourceBindingUniquePtr&& other) noexcept;

        /**
         * Move assignment.
         *
         * @param other Other object.
         *
         * @return Result of move assignment.
         */
        ShaderCpuWriteResourceBindingUniquePtr&
        operator=(ShaderCpuWriteResourceBindingUniquePtr&& other) noexcept;

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
        inline ShaderCpuWriteResourceBinding* getResource() const { return pResource; }

    private:
        /**
         * Constructs a new unique pointer.
         *
         * @param pManager  Manager that owns the resource.
         * @param pResource Resource to point to.
         */
        ShaderCpuWriteResourceBindingUniquePtr(
            ShaderCpuWriteResourceBindingManager* pManager, ShaderCpuWriteResourceBinding* pResource);

        /** Manager that owns the resource we are pointing to. */
        ShaderCpuWriteResourceBindingManager* pManager = nullptr;

        /** Resource we are pointing to. */
        ShaderCpuWriteResourceBinding* pResource = nullptr;
    };
} // namespace ne
