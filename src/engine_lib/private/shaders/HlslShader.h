#pragma once

// Custom.
#include "shaders/IShader.h"

// DXC
#include "dxc/d3d12shader.h"
#include "dxc/dxcapi.h"
#pragma comment(lib, "dxc/dxcompiler.lib")

// OS.
#include <wrl.h>
using namespace Microsoft::WRL;

namespace ne {
    /**
     * HLSL shader.
     */
    class HlslShader : public IShader {
    public:
        HlslShader() = default;

        HlslShader(const HlslShader &) = delete;
        HlslShader &operator=(const HlslShader &) = delete;

        virtual ~HlslShader() override {}

        /**
         * Returns compiled shader.
         *
         * @return Compiled shader blob.
         */
        ComPtr<IDxcBlob> getCompiledBlob();
    };
} // namespace ne
