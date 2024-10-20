#pragma once

namespace ne {
    class ShaderTextureResourceBindingManager;
    class ShaderTextureResourceBinding;

    /**
     * Small raw pointer wrapper that acts like `std::unique_ptr`
     * for shader texture resource bindings to do some extra work
     * when started/stopped referencing a resource.
     *
     * When deleted causes the resource to be also deleted.
     */
    class ShaderTextureResourceBindingUniquePtr {
        // Only manager can create objects of this class.
        friend class ShaderTextureResourceBindingManager;

    public:
        ShaderTextureResourceBindingUniquePtr() = default;

        ShaderTextureResourceBindingUniquePtr(const ShaderTextureResourceBindingUniquePtr&) = delete;
        ShaderTextureResourceBindingUniquePtr&
        operator=(const ShaderTextureResourceBindingUniquePtr&) = delete;

        ~ShaderTextureResourceBindingUniquePtr();

        /**
         * Move constructor.
         *
         * @param other Other object.
         */
        ShaderTextureResourceBindingUniquePtr(ShaderTextureResourceBindingUniquePtr&& other) noexcept;

        /**
         * Move assignment.
         *
         * @param other Other object.
         *
         * @return Result of move assignment.
         */
        ShaderTextureResourceBindingUniquePtr&
        operator=(ShaderTextureResourceBindingUniquePtr&& other) noexcept;

        /**
         * Returns the underlying resource.
         *
         * @return `nullptr` if moved or not initialized, otherwise valid pointer to resource.
         */
        inline ShaderTextureResourceBinding* getResource() const { return pResource; }

    private:
        /**
         * Constructs a new unique pointer.
         *
         * @param pManager  Manager that owns the resource.
         * @param pResource Resource to point to.
         */
        ShaderTextureResourceBindingUniquePtr(
            ShaderTextureResourceBindingManager* pManager, ShaderTextureResourceBinding* pResource);

        /** Manager that owns the resource we are pointing to. */
        ShaderTextureResourceBindingManager* pManager = nullptr;

        /** Resource we are pointing to. */
        ShaderTextureResourceBinding* pResource = nullptr;
    };
} // namespace ne
