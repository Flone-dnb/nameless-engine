#include "io/MeshImporter.h"

// Standard.
#include <format>

// Custom.
#include "misc/ProjectPaths.h"
#include "game/nodes/MeshNode.h"
#include "material/Material.h"

// External.
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tinygltf/tiny_gltf.h"

namespace ne {

#if defined(WIN32)
    inline bool textureImportProcess(float percent, unsigned long long, unsigned long long) { // NOLINT
#else
    inline bool textureImportProcess(float percent, int*, int*) { // NOLINT
#endif
        return false;
    }

    inline bool
    writeGltfTextureToDisk(const tinygltf::Image& image, const std::filesystem::path& pathToImage) {
        // Prepare callbacks.
        tinygltf::FsCallbacks fsCallbacks = {
            &tinygltf::FileExists,
            &tinygltf::ExpandFilePath,
            &tinygltf::ReadWholeFile,
            &tinygltf::WriteWholeFile,
            &tinygltf::GetFileSizeInBytes,
            nullptr};
        tinygltf::URICallbacks uriCallbacks;
        uriCallbacks.encode = nullptr;
        uriCallbacks.decode = [](const std::string& sInUri, std::string* pOutUri, void* pUserData) -> bool {
            *pOutUri = sInUri;
            return true;
        };

        // Prepare paths.
        const auto sFilename = pathToImage.stem().string() + pathToImage.extension().string();
        const auto sBasePath = pathToImage.parent_path().string();
        std::string sOutputUri;

        // Write image to disk.
        return tinygltf::WriteImageData(
            &sBasePath, &sFilename, &image, false, &uriCallbacks, &sOutputUri, &fsCallbacks);
    }

    inline std::variant<Error, gc_vector<MeshNode>> processGltfMesh( // NOLINT: too complex
        const tinygltf::Model& model,
        const tinygltf::Mesh& mesh,
        const std::filesystem::path& pathToOutputFile,
        const std::string& sPathToOutputDirRelativeRes,
        const std::function<void(float, const std::string&)>& onProgress,
        size_t& iGltfNodeProcessedCount) {
        // Prepare an array to fill.
        gc_vector<MeshNode> vMeshNodes = gc_new_vector<MeshNode>();

        // Prepare variables.
        const auto gltfNodePercentRange = 100.0F / static_cast<float>(model.nodes.size());
        const std::string sTexturesDirectoryName = "textures";
        const std::string sImageExtension = ".png";
        const std::string sDiffuseTextureName = "diffuse";
        std::string sPathToImportTexturesRelativeRes = sPathToOutputDirRelativeRes;
        if (!sPathToImportTexturesRelativeRes.ends_with('/')) {
            sPathToImportTexturesRelativeRes += "/";
        }
        sPathToImportTexturesRelativeRes += sTexturesDirectoryName;

        // Prepare paths.
        const std::filesystem::path pathToTempFiles =
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / sPathToOutputDirRelativeRes /
            "temp";
        const std::filesystem::path pathToImportTextures =
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / sPathToImportTexturesRelativeRes;
        if (std::filesystem::exists(pathToTempFiles)) {
            std::filesystem::remove_all(pathToTempFiles);
        }
        std::filesystem::create_directory(pathToTempFiles);
        if (std::filesystem::exists(pathToImportTextures)) {
            std::filesystem::remove_all(pathToImportTextures);
        }
        std::filesystem::create_directory(pathToImportTextures);

        // Go through each mesh in this node.
        for (size_t iPrimitive = 0; iPrimitive < mesh.primitives.size(); iPrimitive++) {
            auto& primitive = mesh.primitives[iPrimitive];

            // Allocate a new mesh data.
            MeshData meshData;

            {
                // Add indices.
                // Save accessor to mesh indices.
                const auto& indexAccessor = model.accessors[primitive.indices];

                // Get index buffer.
                const auto& indexBufferView = model.bufferViews[indexAccessor.bufferView];
                const auto& indexBuffer = model.buffers[indexBufferView.buffer];

                // Make sure index is stored as scalar`.
                if (indexAccessor.type != TINYGLTF_TYPE_SCALAR) [[unlikely]] {
                    return Error(std::format(
                        "expected indices of mesh to be stored as `scalar`, actual type: {}",
                        indexAccessor.type));
                }

                // Prepare variables to read indices.
                auto pCurrentIndex =
                    indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;
                std::vector<MeshData::meshindex_t> vIndices(indexAccessor.count);

                // Allocate indices depending on their type.
                if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    using index_t = unsigned int;

                    const auto iStride =
                        indexBufferView.byteStride == 0 ? sizeof(index_t) : indexBufferView.byteStride;

                    // Set indices.
                    for (size_t i = 0; i < vIndices.size(); i++) {
                        // Set value.
                        vIndices[i] = reinterpret_cast<const index_t*>(pCurrentIndex)[0];

                        // Switch to the next item.
                        pCurrentIndex += iStride;
                    }
                } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    using index_t = unsigned short;

                    const auto iStride =
                        indexBufferView.byteStride == 0 ? sizeof(index_t) : indexBufferView.byteStride;

                    // Set indices.
                    for (size_t i = 0; i < vIndices.size(); i++) {
                        // Set value.
                        vIndices[i] = reinterpret_cast<const index_t*>(pCurrentIndex)[0];

                        // Switch to the next item.
                        pCurrentIndex += iStride;
                    }
                } else {
                    return Error(std::format(
                        "expected indices mesh component type to be `unsigned int` or `unsigned short`, "
                        "actual type: {}",
                        indexAccessor.componentType));
                }

                // Allocate a material slot with indices.
                meshData.getIndices()->push_back(std::move(vIndices));
            }

            {
                // Find a position attribute to know how much vertices there will be.
                const auto it = primitive.attributes.find("POSITION");
                if (it == primitive.attributes.end()) [[unlikely]] {
                    return Error("a GLTF mesh node does not have any positions defined");
                }
                const auto iPositionAccessorIndex = it->second;

                // Get accessor.
                const auto& positionAccessor = model.accessors[iPositionAccessorIndex];

                // Allocate vertices.
                meshData.getVertices()->resize(positionAccessor.count);
            }

            // Process attributes.
            size_t iProcessedAttributeCount = 0;
            for (auto& [sAttributeName, iAccessorIndex] : primitive.attributes) {
                // Mark progress.
                const auto processedAttributes = static_cast<float>(iProcessedAttributeCount) /
                                                 static_cast<float>(primitive.attributes.size());
                onProgress(
                    static_cast<float>(iGltfNodeProcessedCount) / static_cast<float>(model.nodes.size()) *
                            100.0F +
                        gltfNodePercentRange * processedAttributes,
                    std::format(
                        "processing GLTF nodes {}/{} (processing attribute \"{}\")",
                        iGltfNodeProcessedCount,
                        model.nodes.size(),
                        sAttributeName));
                iProcessedAttributeCount += 1;

                // Get attribute accessor.
                const auto& attributeAccessor = model.accessors[iAccessorIndex];

                // Get buffer.
                const auto& attributeBufferView = model.bufferViews[attributeAccessor.bufferView];
                const auto& attributeBuffer = model.buffers[attributeBufferView.buffer];

                if (sAttributeName == "POSITION") {
                    using position_t = glm::vec3;

                    // Make sure position is stored as `vec3`.
                    if (attributeAccessor.type != TINYGLTF_TYPE_VEC3) [[unlikely]] {
                        return Error(std::format(
                            "expected POSITION mesh attribute to be stored as `vec3`, actual type: {}",
                            attributeAccessor.type));
                    }
                    // Make sure that component type is `float`.
                    if (attributeAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) [[unlikely]] {
                        return Error(std::format(
                            "expected POSITION mesh attribute component type to be `float`, actual type: {}",
                            attributeAccessor.componentType));
                    }

                    // Prepare variables.
                    auto pCurrentPosition = attributeBuffer.data.data() + attributeBufferView.byteOffset +
                                            attributeAccessor.byteOffset;
                    const auto iStride = attributeBufferView.byteStride == 0 ? sizeof(position_t)
                                                                             : attributeBufferView.byteStride;

                    // Set positions to mesh data.
                    for (size_t i = 0; i < meshData.getVertices()->size(); i++) {
                        // Set value.
                        meshData.getVertices()->at(i).position =
                            reinterpret_cast<const position_t*>(pCurrentPosition)[0];

                        // Switch to the next item.
                        pCurrentPosition += iStride;
                    }

                    // Process next attribute.
                    continue;
                }

                if (sAttributeName == "NORMAL") {
                    using normal_t = glm::vec3;

                    // Make sure normal is stored as `vec3`.
                    if (attributeAccessor.type != TINYGLTF_TYPE_VEC3) [[unlikely]] {
                        return Error(std::format(
                            "expected NORMAL mesh attribute to be stored as `vec3`, actual type: {}",
                            attributeAccessor.type));
                    }
                    // Make sure that component type is `float`.
                    if (attributeAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) [[unlikely]] {
                        return Error(std::format(
                            "expected NORMAL mesh attribute component type to be `float`, actual type: {}",
                            attributeAccessor.componentType));
                    }

                    // Prepare variables.
                    auto pCurrentNormal = attributeBuffer.data.data() + attributeBufferView.byteOffset +
                                          attributeAccessor.byteOffset;
                    const auto iStride = attributeBufferView.byteStride == 0 ? sizeof(normal_t)
                                                                             : attributeBufferView.byteStride;

                    // Set normals to mesh data.
                    for (size_t i = 0; i < meshData.getVertices()->size(); i++) {
                        // Set value.
                        meshData.getVertices()->at(i).normal =
                            reinterpret_cast<const normal_t*>(pCurrentNormal)[0];

                        // Switch to the next item.
                        pCurrentNormal += iStride;
                    }

                    // Process next attribute.
                    continue;
                }

                if (sAttributeName == "TEXCOORD_0") {
                    using uv_t = glm::vec2;

                    // Make sure UV is stored as `vec2`.
                    if (attributeAccessor.type != TINYGLTF_TYPE_VEC2) [[unlikely]] {
                        return Error(std::format(
                            "expected TEXCOORD mesh attribute to be stored as `vec2`, actual type: {}",
                            attributeAccessor.type));
                    }
                    // Make sure that component type is `float`.
                    if (attributeAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) [[unlikely]] {
                        return Error(std::format(
                            "expected TEXCOORD mesh attribute component type to be `float`, actual type: {}",
                            attributeAccessor.componentType));
                    }

                    // Prepare variables.
                    auto pCurrentUv = attributeBuffer.data.data() + attributeBufferView.byteOffset +
                                      attributeAccessor.byteOffset;
                    const auto iStride =
                        attributeBufferView.byteStride == 0 ? sizeof(uv_t) : attributeBufferView.byteStride;

                    // Set UVs to mesh data.
                    for (size_t i = 0; i < meshData.getVertices()->size(); i++) {
                        // Set value.
                        meshData.getVertices()->at(i).uv = reinterpret_cast<const uv_t*>(pCurrentUv)[0];

                        // Switch to the next item.
                        pCurrentUv += iStride;
                    }

                    // Process next attribute.
                    continue;
                }

                Logger::get().warn(std::format("skipping unsupported GLTF attribute \"{}\"", sAttributeName));
            }

            // See if we generated some mesh data.
            if (meshData.getVertices()->empty() || meshData.getIndices()->empty()) {
                continue;
            }

            // Create a new mesh node with the specified data.
            auto pMeshNode = gc_new<MeshNode>(mesh.name);
            pMeshNode->setMeshData(std::move(meshData));

            if (primitive.material >= 0) {
                // Process material.
                auto& material = model.materials[primitive.material];
                const auto pMeshMaterial = pMeshNode->getMaterial();

                // Process base color.
                pMeshMaterial->setDiffuseColor(glm::vec3(
                    material.pbrMetallicRoughness.baseColorFactor[0],
                    material.pbrMetallicRoughness.baseColorFactor[1],
                    material.pbrMetallicRoughness.baseColorFactor[2]));

                // Process diffuse texture.
                const auto iDiffuseTextureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
                if (iDiffuseTextureIndex >= 0) {
                    auto& diffuseTexture = model.textures[iDiffuseTextureIndex];
                    if (diffuseTexture.source >= 0) {
                        // Get image.
                        auto& diffuseImage = model.images[diffuseTexture.source];

                        // Prepare path to export the image to.
                        const auto pathToDiffuseImage =
                            pathToTempFiles / (sDiffuseTextureName + sImageExtension);

                        // Mark progress.
                        onProgress(
                            static_cast<float>(iGltfNodeProcessedCount) /
                                    static_cast<float>(model.nodes.size()) * 100.0F +
                                gltfNodePercentRange,
                            std::format(
                                "processing GLTF nodes {}/{} (importing diffuse texture)",
                                iGltfNodeProcessedCount,
                                model.nodes.size()));

                        // Write image to disk.
                        if (!writeGltfTextureToDisk(diffuseImage, pathToDiffuseImage)) {
                            return Error(std::format(
                                "failed to write GLTF image to path \"{}\"", pathToDiffuseImage.string()));
                        }

                        // Import texture.
                        auto optionalError = TextureManager::importTexture(
                            pathToDiffuseImage,
                            TextureType::DIFFUSE,
                            sPathToImportTexturesRelativeRes,
                            sDiffuseTextureName,
                            textureImportProcess);
                        if (optionalError.has_value()) [[unlikely]] {
                            auto error = std::move(optionalError.value());
                            error.addCurrentLocationToErrorStack();
                            return error;
                        }

                        // Construct path to imported texture directory.
                        std::string sPathDiffuseTextureRelativeRes = sPathToImportTexturesRelativeRes;
                        if (!sPathDiffuseTextureRelativeRes.ends_with('/')) {
                            sPathDiffuseTextureRelativeRes += "/";
                        }
                        sPathDiffuseTextureRelativeRes += sDiffuseTextureName;

                        // Specify texture path.
                        pMeshMaterial->setDiffuseTexture(sPathDiffuseTextureRelativeRes);
                    }
                }
            }

            // Add this new mesh node to results.
            vMeshNodes->push_back(std::move(pMeshNode));
        }

        // Cleanup.
        std::filesystem::remove_all(pathToTempFiles);

        return vMeshNodes;
    }

    inline std::optional<Error> processGltfNode(
        const tinygltf::Node& node,
        const tinygltf::Model& model,
        const std::filesystem::path& pathToOutputFile,
        const std::string& sPathToOutputDirRelativeRes,
        Node* pParentNode,
        const std::function<void(float, const std::string&)>& onProgress,
        size_t& iGltfNodeProcessedCount) {
        // Prepare a node that will store this GLTF node.
        Node* pThisNode = pParentNode;

        // See if this node stores a mesh.
        if ((node.mesh >= 0) && (node.mesh < static_cast<int>(model.meshes.size()))) {
            // Process mesh.
            auto result = processGltfMesh(
                model,
                model.meshes[node.mesh],
                pathToOutputFile,
                sPathToOutputDirRelativeRes,
                onProgress,
                iGltfNodeProcessedCount);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            auto vMeshNodes = std::get<gc_vector<MeshNode>>(std::move(result));

            // Attach new nodes to parent.
            for (const auto& pMeshNode : *vMeshNodes) {
                // Attach to parent node.
                pParentNode->addChildNode(
                    pMeshNode,
                    Node::AttachmentRule::KEEP_RELATIVE,  // don't change relative location
                    Node::AttachmentRule::KEEP_RELATIVE,  // don't change relative rotation
                    Node::AttachmentRule::KEEP_RELATIVE); // don't change relative scale

                // Mark this node as parent for child GLTF nodes.
                pThisNode = &*pMeshNode;
            }
        }

        // Mark node as processed.
        iGltfNodeProcessedCount += 1;
        onProgress(
            static_cast<float>(iGltfNodeProcessedCount) / static_cast<float>(model.nodes.size()) * 100.0F,
            std::format("processing GLTF nodes {}/{}", iGltfNodeProcessedCount, model.nodes.size()));

        // Process child nodes.
        for (const auto& iNode : node.children) {
            // Process child node.
            auto optionalError = processGltfNode(
                model.nodes[iNode],
                model,
                pathToOutputFile,
                sPathToOutputDirRelativeRes,
                pThisNode,
                onProgress,
                iGltfNodeProcessedCount);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        return {};
    }

    std::optional<Error> MeshImporter::importMesh(
        const std::filesystem::path& pathToFile,
        const std::string& sPathToOutputDirRelativeRes,
        const std::string& sOutputDirectoryName,
        const std::function<void(float, const std::string&)>& onProgress) {
        // Make sure the file has ".GLTF" or ".GLB" extension.
        if (pathToFile.extension() != ".GLTF" && pathToFile.extension() != ".gltf" &&
            pathToFile.extension() != ".GLB" && pathToFile.extension() != ".glb") [[unlikely]] {
            return Error(std::format(
                "only GLTF/GLB file extension is supported for mesh import, the path \"{}\" points to a "
                "non-GLTF file",
                pathToFile.string()));
        }

        // Make sure the specified path to the file exists.
        if (!std::filesystem::exists(pathToFile)) [[unlikely]] {
            return Error(std::format("the specified path \"{}\" does not exists", pathToFile.string()));
        }

        // Construct an absolute path to the output directory.
        const auto pathToOutputDirectoryParent =
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / sPathToOutputDirRelativeRes;

        // Make sure the output directory exists.
        if (!std::filesystem::exists(pathToOutputDirectoryParent)) [[unlikely]] {
            return Error(std::format(
                "expected the specified path output directory \"{}\" to exist",
                pathToOutputDirectoryParent.string()));
        }

        // Make sure the specified directory name is not empty.
        if (sOutputDirectoryName.empty()) [[unlikely]] {
            return Error("expected the specified directory name to not be empty");
        }

        // Make sure the specified directory name is not very long
        // to avoid creating long paths which might be an issue under Windows.
        static constexpr size_t iMaxOutputDirectoryNameLength = 10; // NOLINT
        if (sOutputDirectoryName.size() > iMaxOutputDirectoryNameLength) [[unlikely]] {
            return Error(std::format(
                "the specified name \"{}\" is too long (only {} characters allowed)",
                sOutputDirectoryName,
                iMaxOutputDirectoryNameLength));
        }

        // Make sure the specified directory name is valid (A-z, 0-9).
        for (const auto& character : sOutputDirectoryName) {
            const auto iAsciiCode = static_cast<int>(character);
            if (iAsciiCode < 48 || (iAsciiCode > 57 && iAsciiCode < 65) ||               // NOLINT
                (iAsciiCode > 90 && iAsciiCode < 97) || iAsciiCode > 122) [[unlikely]] { // NOLINT
                return Error(std::format(
                    "character \"{}\" in the name \"{}\" is forbidden and cannon be used",
                    character,
                    sOutputDirectoryName));
            }
        }

        // Make sure the specified resulting directory does not exists yet.
        const auto pathToOutputDirectory = pathToOutputDirectoryParent / sOutputDirectoryName;
        const auto pathToOutputFile = pathToOutputDirectoryParent / sOutputDirectoryName /
                                      (sOutputDirectoryName + ConfigManager::getConfigFormatExtension());
        if (std::filesystem::exists(pathToOutputDirectory)) [[unlikely]] {
            return Error(std::format(
                "expected the resulting directory \"{}\" to not exist", pathToOutputDirectory.string()));
        }

        // Create resulting directory.
        std::filesystem::create_directory(pathToOutputDirectory);

        // See if we have a binary GTLF file or not.
        bool bIsGlb = false;
        if (pathToFile.extension() == ".GLB" || pathToFile.extension() == ".glb") {
            bIsGlb = true;
        }

        using namespace tinygltf;

        // Prepare variables for storing results.
        Model model;
        TinyGLTF loader;
        std::string sError;
        std::string sWarning;
        bool bIsSuccess = false;

        // Don't make all images to be in RGBA format.
        loader.SetPreserveImageChannels(true);

        // Mark progress.
        onProgress(0.0F, "parsing file");

        // Load data from file.
        if (bIsGlb) {
            bIsSuccess = loader.LoadBinaryFromFile(&model, &sError, &sWarning, pathToFile.string());
        } else {
            bIsSuccess = loader.LoadASCIIFromFile(&model, &sError, &sWarning, pathToFile.string());
        }

        // See if there were any warnings/errors.
        if (!sWarning.empty()) {
            // Treat warnings as errors.
            return Error(std::format("there was an error during the import process: {}", sWarning));
        }
        if (!sError.empty()) {
            return Error(std::format("there was an error during the import process: {}", sError));
        }
        if (!bIsSuccess) {
            return Error("there was an error during the import process but no error message was received");
        }

        // Prepare variable for processed nodes.
        size_t iTotalNodeProcessedCount = 0;

        // Get default scene.
        const auto& scene = model.scenes[model.defaultScene];

        // Create a scene root node to hold all GLTF nodes of the scene.
        auto pSceneRootNode = gc_new<Node>("Scene Root");

        for (const auto& iNode : scene.nodes) {
            // Make sure this node index is valid.
            if (iNode < 0) [[unlikely]] {
                return Error(std::format("found a negative node index of {} in default scene", iNode));
            }
            if (iNode >= static_cast<int>(model.nodes.size())) [[unlikely]] {
                return Error(std::format(
                    "found an out of bounds node index of {} while model nodes only has {} entries",
                    iNode,
                    model.nodes.size()));
            }

            // Process node.
            auto optionalError = processGltfNode(
                model.nodes[iNode],
                model,
                pathToOutputFile,
                std::format("{}/{}", sPathToOutputDirRelativeRes, sOutputDirectoryName),
                &*pSceneRootNode,
                onProgress,
                iTotalNodeProcessedCount);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        // Mark progress.
        onProgress(100.0F, "serializing resulting node tree");

        // Serialize scene node tree.
        auto optionalError = pSceneRootNode->serializeNodeTree(pathToOutputFile, false);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Mark progress.
        onProgress(100.0F, "finished");

        return {};
    }

}
