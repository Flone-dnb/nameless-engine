#pragma once

namespace ne {
    class ShadowMapManager;
    class ShadowMap;

    /**
     * Small raw pointer wrapper that acts like `std::unique_ptr`
     * for shadow maps to do some extra work when started/stopped referencing a shadow map.
     *
     * When deleted causes the resource to be also deleted.
     */
    class ShadowMapUniquePtr {
        // Only manager can create objects of this class.
        friend class ShadowMapManager;

    public:
        ShadowMapUniquePtr() = default;

        ShadowMapUniquePtr(const ShadowMapUniquePtr&) = delete;
        ShadowMapUniquePtr& operator=(const ShadowMapUniquePtr&) = delete;

        ~ShadowMapUniquePtr();

        /**
         * Move constructor.
         *
         * @param other Other object.
         */
        ShadowMapUniquePtr(ShadowMapUniquePtr&& other) noexcept;

        /**
         * Move assignment.
         *
         * @param other Other object.
         *
         * @return Result of move assignment.
         */
        ShadowMapUniquePtr& operator=(ShadowMapUniquePtr&& other) noexcept;

        /**
         * Returns the underlying resource.
         *
         * @return `nullptr` if moved or not initialized, otherwise valid pointer to resource.
         */
        inline ShadowMap* getResource() const { return pResource; }

    private:
        /**
         * Constructs a new unique pointer.
         *
         * @param pManager  Manager that owns the resource.
         * @param pResource Resource to point to.
         */
        ShadowMapUniquePtr(ShadowMapManager* pManager, ShadowMap* pResource);

        /** Manager that owns the resource we are pointing to. */
        ShadowMapManager* pManager = nullptr;

        /** Resource we are pointing to. */
        ShadowMap* pResource = nullptr;
    };
}
