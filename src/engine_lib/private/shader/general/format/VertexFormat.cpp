#include "VertexFormat.h"

// Standard.
#include <stdexcept>

// Custom.
#include "misc/Error.h"
#include "render/vulkan/VulkanRenderer.h"
#include "shader/glsl/format/GlslVertexFormatDescription.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "shader/hlsl/format/HlslVertexFormatDescription.h"
#endif

namespace ne {

    std::unique_ptr<VertexFormatDescription>
    VertexFormatDescription::createDescription(VertexFormat type, Renderer* pRenderer) {
        if (dynamic_cast<VulkanRenderer*>(pRenderer) != nullptr) {
            return GlslVertexFormatDescription::createDescription(type);
        }

#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            return HlslVertexFormatDescription::createDescription(type);
        }
#endif

        Error error("unexpected renderer");
        error.showError();
        throw std::runtime_error(error.getFullErrorMessage());
    }

}
