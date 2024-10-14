#include "material/TextureHandle.h"

// Custom.
#include "material/TextureManager.h"

namespace ne {
    TextureHandle::TextureHandle(
        TextureManager* pTextureManager, const std::string& sPathToResourceRelativeRes, GpuResource* pTexture)
        : sPathToResourceRelativeRes(sPathToResourceRelativeRes), pTextureManager(pTextureManager),
          pTexture(pTexture) {}

    GpuResource* TextureHandle::getResource() { return pTexture; }

    std::string TextureHandle::getPathToResourceRelativeRes() { return sPathToResourceRelativeRes; }

    TextureHandle::~TextureHandle() {
        pTextureManager->releaseTextureResourceIfNotUsed(sPathToResourceRelativeRes);
    }
}
