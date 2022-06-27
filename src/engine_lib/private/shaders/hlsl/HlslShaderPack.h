﻿#pragma once

// STL.
#include <unordered_map>
#include <set>
#include <mutex>

// Custom.
#include "shaders/ShaderParameter.h"
#include "shaders/hlsl/HlslShader.h"
#include "shaders/IShaderPack.h"

namespace ne {
    /** Represents a group of different variants of one HLSL shader. */
    class HlslShaderPack : public IShaderPack {
    public:
        /**
         * Creates a new shader pack using shader cache.
         *
         * @param pRenderer            Used renderer.
         * @param pathToCompiledShader Path to compiled shader bytecode on disk.
         * @param sShaderName          Unique name of this shader.
         * @param shaderType           Type of this shader.
         */
        HlslShaderPack(
            IRenderer* pRenderer,
            const std::filesystem::path& pathToCompiledShader,
            const std::string& sShaderName,
            ShaderType shaderType);

        HlslShaderPack() = delete;
        HlslShaderPack(const HlslShaderPack&) = delete;
        HlslShaderPack& operator=(const HlslShaderPack&) = delete;

        virtual ~HlslShaderPack() override = default;

        /**
         * Compiles a shader pack.
         *
         * @param pRenderer         DirectX renderer.
         * @param shaderDescription Description that describes the shader and how the shader should be
         * compiled.
         *
         * @return Returns one of the three values:
         * @arg compiled shader pack
         * @arg string containing shader compilation error/warning
         * @arg internal error
         */
        static std::variant<
            std::shared_ptr<IShaderPack> /** Compiled shader pack. */,
            std::string /** Compilation error. */,
            Error /** Internal error. */>
        compileShader(IRenderer* pRenderer, const ShaderDescription& shaderDescription);

        /**
         * Looks for a shader that matches the specified configuration and returns it.
         *
         * @warning If you are calling this function not the first time,
         * make sure you are not holding any references to the old shader
         * as we will try to release old shader's resources from memory.
         *
         * @param configuration New configuration.
         *
         * @return Empty if the shader for this configuration was not found, otherwise
         * shader that matches this configuration.
         */
        std::optional<std::shared_ptr<IShader>>
        changeConfiguration(const std::set<ShaderParameter>& configuration);

        /**
         * Returns type of this shader.
         *
         * @return Shader type.
         */
        virtual ShaderType getShaderType() override;

        /**
         * Tests if shader cache for this shader pack is corrupted or not
         * and deletes the cache if it's corrupted.
         *
         * @remark This function should be used if you want to use shader cache.
         *
         * @return Error if at least one shader cache is corrupted.
         */
        virtual std::optional<Error> testIfShaderCacheIsCorrupted() override;

        /**
         * Releases underlying shader bytecode for each shader from memory (this object will not be deleted)
         * if the shader bytecode was loaded into memory.
         * Next time this shader will be needed it will be loaded from disk.
         *
         * @param bLogOnlyErrors Specify 'true' to only log errors, 'false' to log errors and info.
         * Specifying 'true' is useful when we are testing if shader cache is corrupted or not,
         * to make log slightly cleaner.
         *
         * @return 'false' if at least one shader variant was released from memory,
         * 'true' if all variants were not loaded into memory.
         */
        virtual bool releaseShaderPackDataFromMemoryIfLoaded(bool bLogOnlyErrors = false) override;

    private:
        /**
         * Constructor.
         *
         * @param sShaderName Name of the shader.
         */
        HlslShaderPack(const std::string& sShaderName);

        /** Mutex for working with shaders. */
        std::mutex mtxShaders;

        /** A shader that was requested in the last call to @ref changeConfiguration. */
        std::optional<std::shared_ptr<IShader>*> previouslyRequestedShader;

        /** Map of shaders in this pack. */
        std::unordered_map<std::set<ShaderParameter>, std::shared_ptr<IShader>, ShaderParameterSetHash>
            shaders;
    };
} // namespace ne
