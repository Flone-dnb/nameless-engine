#pragma once

// Standard.
#include <mutex>

namespace ne {
    class ShaderBindlessTextureResourceManager;
    class ShaderBindlessTextureResource;

    /**
     * Small raw pointer wrapper that acts like `std::unique_ptr`
     * for shader bindless texture resources to do some extra work
     * when started/stopped referencing a resource.
     *
     * When deleted causes the resource to be also deleted.
     */
    class ShaderBindlessTextureResourceUniquePtr {
        // Only manager can create objects of this class.
        friend class ShaderBindlessTextureResourceManager;

    public:
        ShaderBindlessTextureResourceUniquePtr() = default;

        ShaderBindlessTextureResourceUniquePtr(const ShaderBindlessTextureResourceUniquePtr&) = delete;
        ShaderBindlessTextureResourceUniquePtr&
        operator=(const ShaderBindlessTextureResourceUniquePtr&) = delete;

        ~ShaderBindlessTextureResourceUniquePtr();

        /**
         * Move constructor.
         *
         * @param other Other object.
         */
        ShaderBindlessTextureResourceUniquePtr(ShaderBindlessTextureResourceUniquePtr&& other) noexcept;

        /**
         * Move assignment.
         *
         * @param other Other object.
         *
         * @return Result of move assignment.
         */
        ShaderBindlessTextureResourceUniquePtr&
        operator=(ShaderBindlessTextureResourceUniquePtr&& other) noexcept;

        /**
         * Returns the underlying resource.
         *
         * @return `nullptr` if moved or not initialized, otherwise valid pointer to resource.
         */
        inline ShaderBindlessTextureResource* getResource() const { return pResource; }

    private:
        /**
         * Constructs a new unique pointer.
         *
         * @param pManager  Manager that owns the resource.
         * @param pResource Resource to point to.
         */
        ShaderBindlessTextureResourceUniquePtr(
            ShaderBindlessTextureResourceManager* pManager, ShaderBindlessTextureResource* pResource);

        /** Manager that owns the resource we are pointing to. */
        ShaderBindlessTextureResourceManager* pManager = nullptr;

        /** Resource we are pointing to. */
        ShaderBindlessTextureResource* pResource = nullptr;
    };
} // namespace ne
