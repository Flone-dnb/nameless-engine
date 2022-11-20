#pragma once

// Standard.
#include <unordered_map>
#include <memory>
#include <string>
#include <mutex>

namespace ne {
    class DirectXPso;
    class DirectXRenderer;

    /** Creates and manages DirectX pipeline state objects (PSO). */
    class DirectXPsoManager {
    public:
        /**
         * Creates the manager without any pipeline state objects.
         *
         * @param pRenderer Parent renderer.
         */
        DirectXPsoManager(DirectXRenderer* pRenderer);

        DirectXPsoManager() = delete;
        DirectXPsoManager(const DirectXPsoManager&) = delete;
        DirectXPsoManager& operator=(const DirectXPsoManager&) = delete;

    private:
        /** Do not delete. Parent renderer. */
        DirectXRenderer* pRenderer;

        /** Used for accessing @ref createdGraphicsPipelineStateObjects and mtxRenderableComponentsPerPso. */
        std::mutex mtxGraphicsPso;

        /**
         * Map of created graphics pipeline state objects (for usual rendering).
         * Maps a string of vertex and pixel shader names (combined) to a PSO.
         */
        std::unordered_map<std::string, std::unique_ptr<DirectXPso>> createdGraphicsPipelineStateObjects;

        /**
         * Array of renderable components for each PSO.
         * PSO raw pointer references PSO from @ref createdGraphicsPipelineStateObjects.
         */
        // TODO: add this when ECS is implemented
        // std::vector<DirectXPso*, std::vector<RenderableComponent>> renderableComponentsPerPso;
    };
} // namespace ne
