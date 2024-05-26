#include "GlslShaderCacheManager.h"

namespace ne {

    GlslShaderCacheManager::GlslShaderCacheManager(Renderer* pRenderer) : ShaderCacheManager(pRenderer) {}

    std::optional<std::string>
    GlslShaderCacheManager::isLanguageSpecificGlobalCacheOutdated(const ConfigManager& cacheConfig) {
        return {};
    }

    std::optional<Error> GlslShaderCacheManager::writeLanguageSpecificParameters(ConfigManager& cacheConfig) {
        return {};
    }

}
