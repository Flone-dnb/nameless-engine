#pragma once

// Standard.
#include <string>

// Custom.
#include "render/general/pso/PsoManager.h"
#include "io/Serializable.h"
#include "materials/ShaderMacro.h"

#include "Material.generated.h"

namespace ne RNAMESPACE() {
    class MeshNode;

    /** Groups mesh nodes by visibility. */
    struct MeshNodesThatUseThisMaterial {
        /**
         * Returns total number of visible and invisible mesh nodes that use this material.
         *
         * @return Total number of nodes that use this material.
         */
        size_t getTotalSize() const { return visibleMeshNodes.size() + invisibleMeshNodes.size(); }

        /**
         * Tells whether the specified node is already added to be considered.
         *
         * @param pMeshNode Mesh node to check.
         *
         * @return Whether the node is added or not.
         */
        bool isMeshNodeAdded(MeshNode* pMeshNode) {
            auto it = visibleMeshNodes.find(pMeshNode);
            if (it != visibleMeshNodes.end()) {
                return true;
            }

            it = invisibleMeshNodes.find(pMeshNode);
            if (it != invisibleMeshNodes.end()) {
                return true;
            }

            return false;
        }

        /** Visible nodes. */
        std::set<MeshNode*> visibleMeshNodes;

        /** Invisible nodes. */
        std::set<MeshNode*> invisibleMeshNodes;
    };

    /** Defines visual aspects of a mesh. */
    class RCLASS(Guid("a603fa3a-e9c2-4c38-bb4c-76384ef001f4")) Material : public Serializable {
        // Mesh node will notify the material when it's spawned/despawned.
        friend class MeshNode;

    public:
        /** Creates uninitialized material, only used for deserialization, instead use @ref create. */
        Material();

        Material(const Material&) = delete;
        Material& operator=(const Material&) = delete;

        virtual ~Material() override;

        /**
         * Returns total amount of currently created materials.
         *
         * @return Total amount of materials.
         */
        static size_t getTotalMaterialCount();

        /**
         * Creates a new material that uses the specified shaders.
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
        std::pair<std::mutex, MeshNodesThatUseThisMaterial>* getSpawnedMeshNodesThatUseThisMaterial();

        /**
         * Returns material name.
         *
         * @return Material name.
         */
        std::string getMaterialName() const;

        /**
         * Tells whether this material uses transparency or not.
         *
         * @return Whether this material uses transparency or not.
         */
        bool isUsingTransparency() const;

        /**
         * Returns PSO that this material uses.
         *
         * @warning Do not delete returned pointer.
         *
         * @return `nullptr` if PSO was not initialized yet, otherwise used PSO.
         */
        Pso* getUsedPso() const;

    private:
        /** Groups internal data. */
        struct InternalResources {
            /** Used PSO, only valid when the mesh that is using this material is spawned. */
            PsoSharedPtr pUsedPso;

            /**
             * Vertex shader macros that this material enables (i.e. DIFFUSE_TEXTURE if using a diffuse
             * texture).
             */
            std::set<ShaderMacro> materialVertexShaderMacros;

            /**
             * Pixel shader macros that this material enables (i.e. DIFFUSE_TEXTURE if using a diffuse
             * texture).
             */
            std::set<ShaderMacro> materialPixelShaderMacros;

            // TODO: texture resources go here
        };

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
         * Called from MeshNode when a spawned mesh node changed its visibility.
         *
         * @param pMeshNode      Spawned mesh node that is using this material.
         * @param bOldVisibility Old visibility of the mesh.
         */
        void onSpawnedMeshNodeChangedVisibility(MeshNode* pMeshNode, bool bOldVisibility);

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
        std::pair<std::mutex, MeshNodesThatUseThisMaterial> mtxSpawnedMeshNodesThatUseThisMaterial;

        /** Internal data. */
        std::pair<std::mutex, InternalResources> mtxInternalResources;

        /** Do not delete (free) this pointer. PSO manager that the renderer owns. */
        PsoManager* pPsoManager = nullptr;

        /** Name of the vertex shader that this material is using. */
        RPROPERTY(Serialize)
        std::string sVertexShaderName;

        /** Name of the pixel shader that this material is using. */
        RPROPERTY(Serialize)
        std::string sPixelShaderName;

        /** Name of this material. */
        RPROPERTY(Serialize)
        std::string sMaterialName;

        /** Whether this material will use transparency or not. */
        RPROPERTY(Serialize)
        bool bUseTransparency = false;

        /** Name of the category used for logging. */
        inline static const char* sMaterialLogCategory = "Material";

        ne_Material_GENERATED
    };
} // namespace ne RNAMESPACE()

File_Material_GENERATED
