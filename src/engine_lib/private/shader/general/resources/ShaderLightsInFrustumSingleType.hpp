#pragma once

// Standard.
#include <vector>
#include <array>
#include <string>

// Custom.
#include "render/general/resources/frame/FrameResourcesManager.h"

namespace ne{
    class Node;

    /**
     * Groups resources related to light sources of the same type (point/spot/directional) in active camera's
     * frustum and has info about which shader resource is used for these light sources.
     */
    struct ShaderLightsInFrustumSingleType {
        /**
         * Array of light nodes of the same type (if this shader light array object is used only for
         * directional lights then only directional light nodes are stored in this array) where positions
         * of light nodes in this array correspond to positions of their light data from
         * @ref vGpuResources. So light data at index 0 in the array used in shaders is taken from
         * the light node stored in this array at index 0.
         *
         * @remark For example, you can use indices of light nodes in this array to tell the
         * shaders which lights to process in shaders and which to ignore.
         */
        std::vector<Node*> vShaderLightNodeArray;

        /**
         * Stores indices to elements from @ref vLightIndicesInFrustum that are located inside of
         * active camera's frustum.
         */
        std::vector<unsigned int> vLightIndicesInFrustum;

        /**
         * GPU resources that store @ref vLightIndicesInFrustum.
         *
         * @remark Resources in this array are always valid if you specified that you need an index
         * array for this light array (when creating shader light array object) and always have space for at
         * least one item (even if there are no lights spawned) to avoid hitting `nullptr` or have
         * branching when binding resources (when there are no active lights these resources will not be
         * used since counter for light sources will be zero but we will have a valid binding).
         */
        std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
            vGpuResources;

        /**
         * Name of the shader resource that stores indices to lights in camera's frustum.
         *
         * @remark Empty if array of indices is not used (empty for directional lights).
         */
        std::string sShaderResourceName;
    };
}
