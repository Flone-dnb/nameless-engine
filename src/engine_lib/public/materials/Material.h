#pragma once

// Standard.
#include <string>

// Custom.
#include "render/pso/PsoManager.h"

namespace ne {
    class MeshNode;

    /**
     * Defines visual aspects of a mesh.
     */
    class Material {
    public:
        Material() = delete;
        Material(const Material&) = delete;
        Material& operator=(const Material&) = delete;

        ~Material();

        /**
         * Creates a new material that uses default engine shaders.
         *
         * @param bUseTransparency Whether this material will use transparency or not.
         * @param sMaterialName    Name of this material.
         *
         * @return Error if something went wrong, otherwise created material.
         */
        static std::variant<std::shared_ptr<Material>, Error>
        create(bool bUseTransparency, const std::string& sMaterialName = "Material");

        /**
         * Creates a new material that uses custom shaders.
         *
         * @param sVertexShaderName Name of the compiled vertex shader
         * (see ShaderManager::compileShaders) to use.
         * @param sPixelShaderName  Name of the compiled pixel shader
         * (see ShaderManager::compileShaders) to use.
         * @param bUseTransparency  Whether this material will use transparency or not.
         * @param sMaterialName     Name of this material.
         *
         * @return Error if something went wrong, otherwise created material.
         */
        static std::variant<std::shared_ptr<Material>, Error> create(
            const std::string& sVertexShaderName,
            const std::string& sPixelShaderName,
            bool bUseTransparency,
            const std::string& sMaterialName = "Material");

        /**
         * Returns array of mesh nodes that currently use this material.
         * Must be used with mutex.
         *
         * @return Array of mesh nodes.
         */
        std::pair<std::mutex, std::set<MeshNode*>>* getSpawnedMeshNodesThatUseThisMaterial();

        /**
         * Returns material name.
         *
         * @return Material name.
         */
        std::string getName() const;

    private:
        // Mesh node will notify the material when it's spawned/despawned.
        friend class MeshNode;

        /**
         * Creates a new material with the specified name.
         *
         * @remark This constructor should only be used internally (only by this class), use @ref
         * create instead.
         *
         * @param sVertexShaderName Name of the vertex shader that this material is using.
         * @param sPixelShaderName  Name of the pixel shader that this material is using.
         * @param bUseTransparency  Whether this material will use transparency or not.
         * @param pPsoManager       PSO manager that the renderer owns.
         * @param sMaterialName     Name of this material.
         */
        Material(
            const std::string& sVertexShaderName,
            const std::string& sPixelShaderName,
            bool bUseTransparency,
            PsoManager* pPsoManager,
            const std::string& sMaterialName = "Material");

        /**
         * Called from MeshNode when a mesh node that uses this material is being spawned.
         *
         * @param pMeshNode Mesh node that is currently being spawned.
         */
        void onMeshNodeSpawned(MeshNode* pMeshNode);

        /**
         * Called from MeshNode when a spawned mesh node changed its material and started using
         * this material now.
         *
         * @param pMeshNode Spawned mesh node that is using this material.
         */
        void onSpawnedMeshNodeStartedUsingMaterial(MeshNode* pMeshNode);

        /**
         * Called from MeshNode when a spawned mesh node changed its material and now no longer
         * using this material.
         *
         * @param pMeshNode Spawned mesh node that stopped using this material.
         */
        void onSpawnedMeshNodeStoppedUsingMaterial(MeshNode* pMeshNode);

        /**
         * Called from MeshNode when a spawned mesh node that uses this material is being despawned.
         *
         * @param pMeshNode Spawned mesh node that is being despawned.
         */
        void onMeshNodeDespawned(MeshNode* pMeshNode);

        /**
         * Array of spawned mesh nodes that use this material.
         * Must be used with mutex.
         */
        std::pair<std::mutex, std::set<MeshNode*>> mtxSpawnedMeshNodesThatUseThisMaterial;

        /** Used PSO, only valid when the mesh that is using this material is spawned. */
        PsoSharedPtr pUsedPso;

        /** Do not delete (free) this pointer. PSO manager that the renderer owns. */
        PsoManager* pPsoManager = nullptr;

        /**
         * Name of the vertex shader that this material is using.
         */
        std::string sVertexShaderName;

        /**
         * Name of the pixel shader that this material is using.
         */
        std::string sPixelShaderName;

        /** Name of this material. */
        std::string sName;

        /** Whether this material will use transparency or not. */
        bool bUseTransparency = false;

        /** Name of the category used for logging. */
        inline static const char* sMaterialLogCategory = "Material";
    };
} // namespace ne
