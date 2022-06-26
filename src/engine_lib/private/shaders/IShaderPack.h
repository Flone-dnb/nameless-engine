#pragma once

// Custom.
#include "shaders/ShaderDescription.h"

namespace ne {
    /** Represents a group of different variants of one shader. */
    class IShaderPack {
    public:
        /**
         * Constructor.
         *
         * @param sShaderName          Unique name of this shader.
         */
        IShaderPack(const std::string& sShaderName);

        IShaderPack() = delete;
        IShaderPack(const IShaderPack&) = delete;
        IShaderPack& operator=(const IShaderPack&) = delete;

        virtual ~IShaderPack() = default;

        /**
         * Returns unique name of this shader.
         *
         * @return Unique name of this shader.
         */
        std::string getShaderName() const;

        /**
         * Tests if shader cache for this shader pack is corrupted or not
         * and deletes the cache if it's corrupted.
         *
         * @remark This function should be used if you want to use shader cache.
         *
         * @return Error if at least one shader cache is corrupted.
         */
        virtual std::optional<Error> testIfShaderCacheIsCorrupted() = 0;

        /**
         * Returns type of this shader.
         *
         * @return Shader type.
         */
        virtual ShaderType getShaderType() const = 0;

        /**
         * Releases underlying shader bytecode from memory (this object will not be deleted)
         * if the shader bytecode was loaded into memory.
         * Next time this shader will be needed it will be loaded from disk.
         *
         * @param bLogOnlyErrors Specify 'true' to only log errors, 'false' to log errors and info.
         * Specifying 'true' is useful when we are testing if shader cache is corrupted or not,
         * to make log slightly cleaner.
         *
         * @return 'false' if was released from memory, 'true' if not loaded into memory.
         */
        virtual bool releaseShaderDataFromMemoryIfLoaded(bool bLogOnlyErrors = false) = 0;

    private:
        /** Initial shader name (without configuration text). */
        std::string sShaderName;
    };
} // namespace ne
