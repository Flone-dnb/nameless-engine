#pragma once

// Standard.
#include <set>

// Custom.
#include "materials/ShaderUser.h"

namespace ne {
    class Renderer;
    class PsoManager;
    class Material;

    enum class PsoType : size_t {
        PT_OPAQUE = 0,  // OPAQUE is a Windows macro, thus adding a prefix
        PT_TRANSPARENT, // TRANSPARENT is a Windows macro, thus adding a prefix

        // !!! new PSO types go here !!!
        // !!! order of entries in this enum defines draw order !!!

        SIZE // marks the size of this enum, should be the last entry
    };

    /**
     * Base class for render specific Pipeline State Objects (PSOs).
     */
    class Pso : public ShaderUser {
        // Only PSO manager should be able to create PSO.
        friend class PsoManager;

        // Will notify when material is no longer referencing a PSO.
        friend class PsoSharedPtr;

    public:
        Pso() = delete;
        Pso(const Pso&) = delete;
        Pso& operator=(const Pso&) = delete;

        virtual ~Pso() override;

        /**
         * Constructs a unique PSO identifier.
         *
         * @param sVertexShaderName  Name of the vertex shader that PSO is using.
         * @param sPixelShaderName   Name of the pixel shader that PSO is using.
         * @param bUsePixelBlending  Whether the pixels of the mesh that uses the PSO should blend with
         * existing pixels on back buffer or not (for transparency).
         *
         * @return Unique PSO identifier.
         */
        static std::string constructUniquePsoIdentifier(
            const std::string& sVertexShaderName,
            const std::string& sPixelShaderName,
            bool bUsePixelBlending);

        /**
         * Returns name of the vertex shader that this PSO is using.
         *
         * @return Empty if not using vertex shader, otherwise name of the used vertex shader.
         */
        std::string getVertexShaderName();

        /**
         * Returns name of the pixel shader that this PSO is using.
         *
         * @return Empty if not using pixel shader, otherwise name of the used pixel shader.
         */
        std::string getPixelShaderName();

        /**
         * Tells whether this PSO is using pixel blending or not.
         *
         * @return Whether this PSO is using pixel blending or not.
         */
        bool isUsingPixelBlending() const;

        /**
         * Returns an array of materials that currently reference this PSO.
         * Must be used with mutex.
         *
         * @return Array of materials that currently reference this PSO.
         */
        std::pair<std::mutex, std::set<Material*>>* getMaterialsThatUseThisPso();

    protected:
        /**
         * Creates a new uninitialized PSO.
         *
         * @param pRenderer         Current renderer.
         * @param pPsoManager       PSO manager that owns this PSO.
         * @param sVertexShaderName Name of the compiled vertex shader (see ShaderManager::compileShaders).
         * @param sPixelShaderName  Name of the compiled pixel shader (see ShaderManager::compileShaders).
         * @param bUsePixelBlending Whether the pixels of the mesh that uses this PSO should blend with
         * existing pixels on back buffer or not (for transparency).
         */
        Pso(Renderer* pRenderer,
            PsoManager* pPsoManager,
            const std::string& sVertexShaderName,
            const std::string& sPixelShaderName,
            bool bUsePixelBlending);

        /**
         * Returns renderer that owns this PSO.
         *
         * @return Renderer.
         */
        Renderer* getRenderer() const;

        /**
         * Returns unique identifier for this PSO.
         *
         * @return Unique PSO identifier.
         */
        std::string getUniquePsoIdentifier() const;

        /**
         * Releases internal resources such as root signature, internal PSO, etc.
         *
         * @warning Expects that the GPU is not referencing this PSO (command queue is empty) and
         * that no drawing will occur until @ref restoreInternalResources is called.
         *
         * @remark Typically used before changing something (for ex. shader configuration), so that no PSO
         * will reference old resources, to later call @ref restoreInternalResources.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> releaseInternalResources() = 0;

        /**
         * Creates internal resources using the current configuration.
         *
         * @remark Called after @ref releaseInternalResources to create resources that will now reference
         * changed (new) resources.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> restoreInternalResources() = 0;

    private:
        /**
         * Assigns vertex and pixel shaders to create a render specific graphics PSO (for usual rendering).
         *
         * @param pRenderer Parent renderer that owns this PSO.
         * @param pPsoManager PSO manager that owns this PSO.
         * @param sVertexShaderName Name of the compiled vertex shader (see ShaderManager::compileShaders).
         * @param sPixelShaderName  Name of the compiled pixel shader (see ShaderManager::compileShaders).
         * @param bUsePixelBlending Whether the pixels of the mesh that uses this PSO should blend with
         * existing pixels on back buffer or not (for transparency).
         *
         * @return Error if one or both were not found in ShaderManager or if failed to generate PSO,
         * otherwise created PSO.
         */
        static std::variant<std::shared_ptr<Pso>, Error> createGraphicsPso(
            Renderer* pRenderer,
            PsoManager* pPsoManager,
            const std::string& sVertexShaderName,
            const std::string& sPixelShaderName,
            bool bUsePixelBlending);

        /**
         * Called to notify this PSO that a material started storing
         * a shared pointer to this PSO.
         *
         * @param pMaterial Material that started using this PSO.
         *
         * @remark When a material is no longer references the PSO use @ref onMaterialNoLongerUsingPso
         */
        void onMaterialUsingPso(Material* pMaterial);

        /**
         * Called to notify this PSO that the shared pointer to this PSO
         * (that Material stores) is now `nullptr`.
         *
         * @param pMaterial Material that stopped using this PSO.
         *
         * @warning Call this function after clearing (setting to `nullptr`) the shared pointer,
         * not before.
         */
        void onMaterialNoLongerUsingPso(Material* pMaterial);

        /**
         * Array of materials that currently reference this PSO.
         * Must be used with mutex.
         */
        std::pair<std::mutex, std::set<Material*>> mtxMaterialsThatUseThisPso;

        /** Do not delete (free) this pointer. PSO manager that owns this PSO. */
        PsoManager* pPsoManager;

        /** Do not delete (free) this pointer. Current renderer. */
        Renderer* pRenderer;

        /**
         * Contains combines used shader names, transparency setting and etc. that
         * uniquely identifies the PSO.
         */
        std::string sUniquePsoIdentifier;

        /** Name of the compiled vertex shader (see ShaderManager::compileShaders) that this PSO uses. */
        std::string sVertexShaderName;

        /** Name of the compiled pixel shader (see ShaderManager::compileShaders) that this PSO uses. */
        std::string sPixelShaderName;

        /** Whether this PSO is using pixel blending or not. */
        bool bIsUsingPixelBlending = false;

        /** Name of the category used for logging. */
        inline static const char* sPsoLogCategory = "Pipeline State Object";
    };
} // namespace ne
