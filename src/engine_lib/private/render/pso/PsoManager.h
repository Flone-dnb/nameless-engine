#pragma once

// Standard.
#include <array>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <atomic>

// Custom.
#include "render/pso/Pso.h"

namespace ne {
    class Renderer;
    class Material;
    class MeshNode;

    /**
     * Small wrapper class for `std::shared_ptr<Pso>` to do some extra work
     * when started/stopped referencing a PSO.
     */
    class PsoSharedPtr {
    public:
        /**
         * Constructs the pointer.
         *
         * @param pPso                 PSO to store.
         * @param pMaterialThatUsesPso Material that stores this pointer.
         */
        explicit PsoSharedPtr(std::shared_ptr<Pso> pPso, Material* pMaterialThatUsesPso) {
            initialize(std::move(pPso), pMaterialThatUsesPso);
        }

        /** Leaves the internal pointers uninitialized. */
        PsoSharedPtr() = default;

        ~PsoSharedPtr() { clearPointerAndNotifyPso(); }

        PsoSharedPtr(const PsoSharedPtr&) = delete;
        PsoSharedPtr& operator=(const PsoSharedPtr&) = delete;

        /**
         * Move constructor.
         *
         * @param other other object.
         */
        PsoSharedPtr(PsoSharedPtr&& other) noexcept { *this = std::move(other); }

        /**
         * Move assignment.
         *
         * @param other other object.
         *
         * @return Result of move assignment.
         */
        PsoSharedPtr& operator=(PsoSharedPtr&& other) noexcept {
            if (this != &other) {
                if (other.pPso != nullptr) {
                    pPso = std::move(other.pPso);
                    other.pPso = nullptr;
                }

                pMaterialThatUsesPso = other.pMaterialThatUsesPso;
                other.pMaterialThatUsesPso = nullptr;
            }

            return *this;
        }

        /**
         * Tells whether the internal PSO was set or not.
         *
         * @return Whether the internal PSO was set or not.
         */
        bool isInitialized() const { return pPso != nullptr; }

        /** Clears the pointer (sets to `nullptr`). */
        void clear() { clearPointerAndNotifyPso(); }

        /**
         * Changes stored PSO to another one.
         *
         * @param pPso                 PSO to use.
         * @param pMaterialThatUsesPso Material that stores this pointer.
         */
        void set(std::shared_ptr<Pso> pPso, Material* pMaterialThatUsesPso) {
            clearPointerAndNotifyPso();
            initialize(std::move(pPso), pMaterialThatUsesPso);
        }

        /**
         * Access operator.
         *
         * @return Raw pointer to the underlying PSO.
         */
        Pso* operator->() const { return pPso.get(); }

    private:
        /** Clears stored shared pointer and notifies the PSO that we no longer reference it. */
        void clearPointerAndNotifyPso() {
            if (this->pPso != nullptr) {
                Pso* pPsoRaw = this->pPso.get();
                this->pPso = nullptr; // clear shared pointer before calling notify function
                pPsoRaw->onMaterialNoLongerUsingPso(pMaterialThatUsesPso);
            }
        }

        /**
         * Initializes internal state.
         *
         * @param pPso                 PSO to store.
         * @param pMaterialThatUsesPso Material that stores this pointer.
         */
        void initialize(std::shared_ptr<Pso> pPso, Material* pMaterialThatUsesPso) {
            this->pPso = std::move(pPso);
            this->pPso->onMaterialUsingPso(pMaterialThatUsesPso);
        }

        /** Internally stored PSO. */
        std::shared_ptr<Pso> pPso = nullptr;

        /** Material that stores this pointer. */
        Material* pMaterialThatUsesPso = nullptr;
    };

    /** Base class for managing render specific Pipeline State Objects (PSOs). */
    class PsoManager {
    public:
        /**
         * Creates a new PSO manager.
         *
         * @param pRenderer Renderer that owns this PSO manager.
         */
        PsoManager(Renderer* pRenderer);

        /** Checks that there are no existing PSOs left. */
        virtual ~PsoManager();

        PsoManager() = delete;
        PsoManager(const PsoManager&) = delete;
        PsoManager& operator=(const PsoManager&) = delete;

        /**
         * Look for already created PSO that uses the specified shaders and settings and returns it,
         * otherwise creates a new PSO.
         *
         * @remark If creating a new PSO, loads the specified shaders from disk into the memory,
         * they will be released from the memory once the PSO object is destroyed (not the shared pointer)
         * and no other object is using them.
         *
         * @param sVertexShaderName Name of the compiled vertex shader (see ShaderManager::compileShaders).
         * @param sPixelShaderName  Name of the compiled pixel shader (see ShaderManager::compileShaders).
         * @param bUsePixelBlending Whether the pixels of the mesh that uses this PSO should blend with
         * existing pixels on back buffer or not (for transparency).
         * @param pMaterial         Material that requests the PSO.
         *
         * @return Error if one or both shaders were not found in ShaderManager or if failed to generate PSO,
         * otherwise created PSO.
         */
        std::variant<PsoSharedPtr, Error> getGraphicsPsoForMaterial(
            const std::string& sVertexShaderName,
            const std::string& sPixelShaderName,
            bool bUsePixelBlending,
            Material* pMaterial);

        /**
         * Returns an array currently existing graphics PSOs that contains a map per PSO type where
         * each map contains a unique PSO identifier string and a graphics PSO. Must be used with mutex.
         *
         * @return Array of currently existing graphics PSOs.
         */
        std::array<
            std::pair<std::recursive_mutex, std::unordered_map<std::string, std::shared_ptr<Pso>>>,
            static_cast<size_t>(PsoType::SIZE)>*
        getGraphicsPsos();

        /**
         * Returns the total amount of currently created graphics PSOs.
         *
         * @return Amount of currently created graphics PSOs.
         */
        size_t getCreatedGraphicsPsoCount();

        /**
         * Returns the total amount of currently created compute PSOs.
         *
         * @return Amount of currently created compute PSOs.
         */
        size_t getCreatedComputePsoCount();

    protected:
        // PSO notifies the manager when a material stops referencing it.
        friend class Pso;

        /**
         * Returns renderer that owns this PSO manager.
         *
         * @return Renderer.
         */
        Renderer* getRenderer() const;

    private:
        /**
         * Assigns vertex and pixel shaders to create a render specific graphics PSO (for usual rendering).
         *
         * @param sVertexShaderName Name of the compiled vertex shader (see ShaderManager::compileShaders).
         * @param sPixelShaderName  Name of the compiled pixel shader (see ShaderManager::compileShaders).
         * @param bUsePixelBlending Whether the pixels of the mesh that uses this PSO should blend with
         * existing pixels on back buffer or not (for transparency).
         * @param pMaterial         Material that requests the PSO.
         *
         * @return Error if one or both were not found in ShaderManager or if failed to generate PSO,
         * otherwise created PSO.
         */
        std::variant<PsoSharedPtr, Error> createGraphicsPsoForMaterial(
            const std::string& sVertexShaderName,
            const std::string& sPixelShaderName,
            bool bUsePixelBlending,
            Material* pMaterial);

        /**
         * Called from a PSO when a material is no longer using a PSO (for ex. because changing shaders).
         *
         * @param sUniquePsoIdentifier Unique PSO identifier string.
         */
        void onPsoNoLongerUsedByMaterial(const std::string& sUniquePsoIdentifier);

        /**
         * Array that contains a map per PSO type where each map contains a unique PSO identifier string
         * and a graphics PSO. Must be used with mutex.
         */
        std::array<
            std::pair<std::recursive_mutex, std::unordered_map<std::string, std::shared_ptr<Pso>>>,
            static_cast<size_t>(PsoType::SIZE)>
            vGraphicsPsos;

        /**
         * Map that contains compute shader name and a compute PSO.
         * Mutex be used with mutex.
         */
        std::pair<std::recursive_mutex, std::unordered_map<std::string, std::shared_ptr<Pso>>> mtxComputePsos;

        /** Do not delete (free) this pointer. Renderer that owns this PSO manager. */
        Renderer* pRenderer;

        /** Name of the category used for logging. */
        inline static const char* sPsoManagerLogCategory = "PSO Manager";
    };
} // namespace ne
