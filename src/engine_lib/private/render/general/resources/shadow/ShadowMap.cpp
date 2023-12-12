#include "ShadowMap.h"

namespace ne {

    std::variant<std::unique_ptr<ShadowMap>, Error> ShadowMap::create(
        GpuResourceManager* pResourceManager, const std::string& sShadowMapName, unsigned int iTextureSize) {
        // TODO
        return Error("not implemented");
    }

    ShadowMap::ShadowMap(std::unique_ptr<GpuResource> pShadowMapResource) {
        this->pShadowMapResource = std::move(pShadowMapResource);
    }

}
