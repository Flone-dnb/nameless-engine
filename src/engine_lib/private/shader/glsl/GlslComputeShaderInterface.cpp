#include "GlslComputeShaderInterface.h"

// Custom.
#include "render/Renderer.h"

namespace ne {
    GlslComputeShaderInterface::GlslComputeShaderInterface(
        Renderer* pRenderer, const std::string& sComputeShaderName, bool bRunBeforeFrameRendering)
        : ComputeShaderInterface(pRenderer, sComputeShaderName, bRunBeforeFrameRendering) {}

}
