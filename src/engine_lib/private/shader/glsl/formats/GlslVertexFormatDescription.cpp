#include "GlslVertexFormatDescription.h"

// Standard.
#include <stdexcept>

// Custom.
#include "misc/Error.h"
#include "MeshNodeGlslVertexFormatDescription.h"

namespace ne {

    std::unique_ptr<GlslVertexFormatDescription> GlslVertexFormatDescription::createDescription(VertexFormat type) {
        switch (type) {
        case (VertexFormat::MESH_NODE): {
            return std::make_unique<MeshNodeGlslVertexFormatDescription>();
            break;
        }
        }

        Error error("unhandled case");
        error.showError();
        throw std::runtime_error(error.getFullErrorMessage());
    }

}
