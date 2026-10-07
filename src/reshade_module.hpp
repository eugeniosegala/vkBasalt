#ifndef RESHADE_MODULE_HPP_INCLUDED
#define RESHADE_MODULE_HPP_INCLUDED

#include <climits>
#include <cstdio>
#include <algorithm>
#include <unordered_map>
#include "stb_image.h"
#include "stb_image_dds.h"
#include <memory>
#include <stdexcept>
#include "config.hpp"
#include "reshade/effect_parser.hpp"
#include "reshade/effect_codegen.hpp"
#include "reshade/effect_preprocessor.hpp"

namespace vkBasalt
{
    struct ReshadeTexture
    {
        int width = 0;
        int height = 0;
        std::vector<stbi_uc> pixels;
    };

    struct PreparedReshadeModule
    {
        reshadefx::module module;
        std::unordered_map<std::string, ReshadeTexture> textures;
    };

    inline PreparedReshadeModule compileReshadeModule(Config& config, const std::string& effectName,
                                                 VkExtent2D extent, bool tenBit)
    {
        reshadefx::preprocessor preprocessor;
        preprocessor.add_macro_definition("__RESHADE__", std::to_string(INT_MAX));
        preprocessor.add_macro_definition("__RESHADE_PERFORMANCE_MODE__", "1");
        preprocessor.add_macro_definition("__RENDERER__", "0x20000");
        preprocessor.add_macro_definition("BUFFER_WIDTH", std::to_string(extent.width));
        preprocessor.add_macro_definition("BUFFER_HEIGHT", std::to_string(extent.height));
        preprocessor.add_macro_definition("BUFFER_RCP_WIDTH", "(1.0 / BUFFER_WIDTH)");
        preprocessor.add_macro_definition("BUFFER_RCP_HEIGHT", "(1.0 / BUFFER_HEIGHT)");
        preprocessor.add_macro_definition("BUFFER_COLOR_DEPTH", tenBit ? "10" : "8");
        const auto includePath = config.getOption<std::string>("reshadeIncludePath");
        if (!includePath.empty())
            preprocessor.add_include_path(includePath);
        const auto path = config.getOption<std::string>(effectName);
        if (path.empty() || !preprocessor.append_file(path))
            throw std::runtime_error("failed to load shader " + effectName + ": " + path + " " + preprocessor.errors());
        if (!preprocessor.errors().empty())
            Logger::warn(preprocessor.errors());

        reshadefx::parser parser;
        std::unique_ptr<reshadefx::codegen> codegen(reshadefx::create_codegen_spirv(true, true, true, true));
        if (!parser.parse(std::move(preprocessor.output()), codegen.get()))
            throw std::runtime_error(parser.errors());
        reshadefx::module module;
        codegen->write_result(module);
        if (module.spirv.empty() || module.techniques.empty() || module.techniques[0].passes.empty())
            throw std::runtime_error("shader has no usable technique: " + effectName);
        PreparedReshadeModule prepared;
        for (const auto& texture : module.textures)
        {
            const auto source = std::find_if(texture.annotations.begin(), texture.annotations.end(),
                                            [](const auto& annotation) { return annotation.name == "source"; });
            if (source == texture.annotations.end() || texture.semantic == "COLOR" || texture.semantic == "DEPTH")
                continue;
            int desiredChannels = 4;
            if (texture.format == reshadefx::texture_format::r8)
                desiredChannels = 1;
            else if (texture.format != reshadefx::texture_format::rg8 && texture.format != reshadefx::texture_format::rgba8)
                throw std::runtime_error("unsupported source texture format: " + texture.unique_name);
            constexpr size_t maxTextureBytes = 256u * 1024u * 1024u;
            if (texture.width == 0 || texture.height == 0 || texture.width > 16384 || texture.height > 16384 ||
                static_cast<uint64_t>(texture.width) * texture.height * desiredChannels > maxTextureBytes)
                throw std::runtime_error("source texture exceeds supported bounds: " + texture.unique_name);
            const auto path = config.getOption<std::string>("reshadeTexturePath") + "/" + source->value.string_data;
            const auto closeFile = [](FILE* file) { fclose(file); };
            std::unique_ptr<FILE, decltype(closeFile)> file(fopen(path.c_str(), "rb"), closeFile);
            if (!file)
                throw std::runtime_error("failed to load texture: " + path);
            ReshadeTexture loaded;
            int channels = 0;
            const bool dds = stbi_dds_test_file(file.get());
            if (!dds && (!stbi_info_from_file(file.get(), &loaded.width, &loaded.height, &channels) ||
                loaded.width <= 0 || loaded.height <= 0 || channels < 1 || channels > 4 || loaded.width > 16384 || loaded.height > 16384 ||
                static_cast<uint64_t>(loaded.width) * loaded.height * std::max(channels, desiredChannels) > maxTextureBytes))
                throw std::runtime_error("invalid or oversized source texture: " + path);
            std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
                dds
                    ? stbi_dds_load_from_file(file.get(), &loaded.width, &loaded.height, &channels, desiredChannels)
                    : stbi_load_from_file(file.get(), &loaded.width, &loaded.height, &channels, desiredChannels),
                stbi_image_free);
            if (!pixels || loaded.width <= 0 || loaded.height <= 0)
                throw std::runtime_error("failed to decode texture: " + path);
            const size_t size = static_cast<size_t>(loaded.width) * loaded.height * desiredChannels;
            loaded.pixels.assign(pixels.get(), pixels.get() + size);
            prepared.textures.emplace(texture.unique_name, std::move(loaded));
        }
        prepared.module = std::move(module);
        return prepared;
    }
}

#endif
