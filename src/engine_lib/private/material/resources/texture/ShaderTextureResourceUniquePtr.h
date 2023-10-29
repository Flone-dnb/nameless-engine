#pragma once

// Standard.
#include <mutex>

namespace ne {
    class ShaderTextureResourceManager;
    class ShaderTextureResource;

    /**
     * Small raw pointer wrapper that acts like `std::unique_ptr`
     * for shader texture resources to do some extra work
     * when started/stopped referencing a resource.
     *
     * When deleted causes the resource to be also deleted.
     */
    class ShaderTextureResourceUniquePtr {
        // Only manager can create objects of this class.
        friend class ShaderTextureResourceManager;

    public:
        ShaderTextureResourceUniquePtr() = default;

        ShaderTextureResourceUniquePtr(const ShaderTextureResourceUniquePtr&) = delete;
        ShaderTextureResourceUniquePtr& operator=(const ShaderTextureResourceUniquePtr&) = delete;

        ~ShaderTextureResourceUniquePtr();

        /**
         * Move constructor.
         *
         * @param other Other object.
         */
        ShaderTextureResourceUniquePtr(ShaderTextureResourceUniquePtr&& other) noexcept;

        /**
         * Move assignment.
         *
         * @param other Other object.
         *
         * @return Result of move assignment.
         */
        ShaderTextureResourceUniquePtr& operator=(ShaderTextureResourceUniquePtr&& other) noexcept;

        /**
         * Returns the underlying resource.
         *
         * @return `nullptr` if moved or not initialized, otherwise valid pointer to resource.
         */
        inline ShaderTextureResource* getResource() const { return pResource; }

    private:
        /**
         * Constructs a new unique pointer.
         *
         * @param pManager  Manager that owns the resource.
         * @param pResource Resource to point to.
         */
        ShaderTextureResourceUniquePtr(
            ShaderTextureResourceManager* pManager, ShaderTextureResource* pResource);

        /** Manager that owns the resource we are pointing to. */
        ShaderTextureResourceManager* pManager = nullptr;

        /** Resource we are pointing to. */
        ShaderTextureResource* pResource = nullptr;
    };
} // namespace ne
