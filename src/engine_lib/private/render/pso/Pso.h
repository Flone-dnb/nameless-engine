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
        PT_OPAQUE = 0,  // OPAQUE is a Windows macro, thus add a prefix
        PT_TRANSPARENT, // TRANSPARENT is a Windows macro, thus add a prefix

        // !!! new PSO types go here !!!
        // !!! order of entries in this enum defines draw order !!!

        SIZE // marks the size of this enum, should be last enum entry
    };

    /**
     * Base class for render specific Pipeline State Objects (PSOs).
     */
    class Pso : public ShaderUser {
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
         * Returns an array of materials that currently reference this PSO.
         * Must be used with mutex.
         *
         * @return Array of materials that currently reference this PSO.
         */
        std::pair<std::mutex, std::set<Material*>>* getMaterialsThatUseThisPso();

    protected:
        // Only PSO manager should be able to create PSO.
        friend class PsoManager;

        // Pso shared pointer will notify when material is no longer referencing a PSO.
        friend class PsoSharedPtr;

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
         * Returns unique identifier for this PSO.
         *
         * @return Unique PSO identifier.
         */
        std::string getUniquePsoIdentifier() const;

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
        std::pair<std::mutex, std::set<Material*>> materialsThatUseThisPso;

        /**
         * Contains combines used shader names, transparency setting and etc. that
         * uniquely identifies the PSO.
         */
        std::string sUniquePsoIdentifier;

        /** Do not delete (free) this pointer. PSO manager that owns this PSO. */
        PsoManager* pPsoManager;

        /** Do not delete (free) this pointer. Current renderer. */
        Renderer* pRenderer;

        /** Name of the category used for logging. */
        inline static const char* sPsoLogCategory = "Pipeline State Object";
    };
} // namespace ne
