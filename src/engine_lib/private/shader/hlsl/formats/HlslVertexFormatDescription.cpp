#include "HlslVertexFormatDescription.h"

// Standard.
#include <stdexcept>

// Custom.
#include "misc/Error.h"
#include "MeshNodeHlslVertexFormatDescription.h"

namespace ne {

    std::unique_ptr<HlslVertexFormatDescription>
    HlslVertexFormatDescription::createDescription(VertexFormat type) {
        switch (type) {
        case (VertexFormat::MESH_NODE): {
            return std::make_unique<MeshNodeHlslVertexFormatDescription>();
            break;
        }
        }

        Error error("unhandled case");
        error.showError();
        throw std::runtime_error(error.getFullErrorMessage());
    }

}
