#pragma once

// Standard.
#include <mutex>
#include <unordered_map>
#include <memory>
#include <optional>
#include <string>

// Custom.
#include "shader/ShaderDescription.h"
#include "shader/general/ShaderMacro.h"

namespace ne {
    class Renderer;
    class Shader;

    /**
     * Represents a group of different variants of one shader
     * (typically this means one shader compiled with different combinations of predefined macros).
     */
    class ShaderPack {
        // Shader manager can change shader pack configuration (to tell what shader should be currently used).
        friend class ShaderManager;

    public:
        /** Groups used data. */
        struct InternalResources {
            InternalResources() = default;

            /** Whether @ref renderConfiguration was set or not. */
            bool bIsRenderConfigurationSet = false;

            /** Last set renderer's configuration. */
            std::set<ShaderMacro> renderConfiguration;

            /** Stores shaders of this pack (pairs of "shader configuration" - "shader"). */
            std::unordered_map<std::set<ShaderMacro>, std::shared_ptr<Shader>, ShaderMacroSetHash>
                shadersInPack;
        };

        ShaderPack() = delete;
        ShaderPack(const ShaderPack&) = delete;
        ShaderPack& operator=(const ShaderPack&) = delete;

        virtual ~ShaderPack() = default;

        /**
         * Compiles a shader pack.
         *
         * @param pRenderer         Used renderer.
         * @param shaderDescription Description that describes the shader and how the shader should be
         * compiled.
         *
         * @return One of the three values: compiled shader pack, string containing shader
         * compilation error/warning, internal error
         */
        static std::variant<std::shared_ptr<ShaderPack>, std::string, Error>
        compileShaderPack(Renderer* pRenderer, const ShaderDescription& shaderDescription);

        /**
         * Creates a new shader pack using shader cache on the disk.
         *
         * @param pRenderer             Used renderer.
         * @param shaderDescription     Description that describes the shader and how the shader should be
         * compiled. Used for cache invalidation.
         * @param cacheInvalidationReason Will be not empty if cache was invalidated. Used for testing.
         *
         * @return Error if shader cache is corrupted or invalid, this also means that
         * corrupted/invalid shader cache directory was deleted and cache for this shader no longer
         * exists, otherwise a shader pack created using cache.
         */
        static std::variant<std::shared_ptr<ShaderPack>, Error> createFromCache(
            Renderer* pRenderer,
            const ShaderDescription& shaderDescription,
            std::optional<ShaderCacheInvalidationReason>& cacheInvalidationReason);

        /**
         * Releases underlying shader bytecode for each shader from memory (this object will not be deleted)
         * if the shader bytecode was loaded into memory.
         * Next time this shader will be needed it will be loaded from disk.
         *
         * @return `false` if at least one shader variant was released from memory,
         * `true` if all variants were not loaded into memory.
         */
        bool releaseShaderPackDataFromMemoryIfLoaded();

        /**
         * Returns a shader that matches the current renderer's shader configuration and the specified
         * additional configuration.
         *
         * @remark Since renderer's shader configuration usually does not contain all needed macros for a
         * shader, you should specify an additional configuration that will be considered together with the
         * renderer's configuration to find a matching shader that has uses this configuration (macros).
         *
         * @remark Some macros that the renderer defines in the current renderer shader configuration
         * will not be appended to the specified configuration if they are not applicable
         * (i.e. if the renderer defines texture filtering macro but your configuration does not
         * have a diffuse texture macro - adding texture filtering macro is useless)
         * see ShaderMacro::isMacroShouldBeConsideredInConfiguration.
         *
         * @remark If the a shader that matches the target configuration is not found an error
         * will be shown and an exception will be thrown.
         *
         * @param additionalConfiguration Macros that the renderer does not define but that are
         * needed by the current configuration for the shader. If this array contains a macro that
         * the renderer already defines an error will be shown and an exception will be thrown.
         * @param fullShaderConfiguration Output. Full shader configuration (might include renderer's
         * configuration) of the found shader.
         *
         * @return Found shader.
         */
        std::shared_ptr<Shader> getShader(
            const std::set<ShaderMacro>& additionalConfiguration,
            std::set<ShaderMacro>& fullShaderConfiguration);

        /**
         * Returns unique name of this shader.
         *
         * @return Unique name of this shader.
         */
        std::string getShaderName() const;

        /**
         * Returns type of this shader.
         *
         * @return Shader type.
         */
        ShaderType getShaderType();

        /**
         * Returns internal resources that this shader pack uses.
         *
         * @return Internal resources.
         */
        std::pair<std::mutex, InternalResources>* getInternalResources();

    private:
        /**
         * Adds additional defined macros to shader description that engine shaders expect.
         *
         * @param description               Shader description to modify.
         * @param shaderConfigurationMacros Macros of the current shader configuration to add.
         * @param pRenderer                 Used renderer.
         */
        static void addEngineMacrosToShaderDescription(
            ShaderDescription& description,
            const std::set<ShaderMacro>& shaderConfigurationMacros,
            Renderer* pRenderer);

        /**
         * Constructor to create an empty shader pack.
         *
         * @param sShaderName Initial name of the shader.
         * @param shaderType  Type of shaders this pack stores.
         */
        ShaderPack(const std::string& sShaderName, ShaderType shaderType);

        /**
         * Sets renderer's shader configuration, it will be considered in the further calls to @ref getShader.
         *
         * @warning If the configuration is changed we will try to release
         * old shader's resources from the memory.
         * Make sure no object is holding shared pointers to old shader (old configuration),
         * otherwise there would be an error printed in the logs.
         *
         * @param renderConfiguration New renderer configuration.
         */
        void setRendererConfiguration(const std::set<ShaderMacro>& renderConfiguration);

        /** Used data. */
        std::pair<std::mutex, InternalResources> mtxInternalResources;

        /** Initial shader name (without configuration text). */
        std::string sShaderName;

        /** Type of shaders this pack stores. */
        ShaderType shaderType;
    };
} // namespace ne
