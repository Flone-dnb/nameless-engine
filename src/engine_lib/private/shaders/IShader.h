#pragma once

namespace ne {
    /**
     * Interface class for different types of shaders.
     */
    class IShader {
    public:
        IShader() = default;

        IShader(const IShader &) = delete;
        IShader &operator=(const IShader &) = delete;

        virtual ~IShader() = default;
    };
} // namespace ne
