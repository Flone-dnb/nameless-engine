#pragma once

namespace ne {
    class Shader {
    public:
        Shader() = default;

        Shader(const Shader &) = delete;
        Shader &operator=(const Shader &) = delete;

        virtual ~Shader() = default;
    };
} // namespace ne
