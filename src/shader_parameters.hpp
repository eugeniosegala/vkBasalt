#ifndef VKBASALT_SHADER_PARAMETERS_HPP
#define VKBASALT_SHADER_PARAMETERS_HPP
#include <string>
#include <unordered_map>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <locale>
#include "config.hpp"
#include "reshade/effect_module.hpp"
namespace vkBasalt {
inline std::string shaderParameterKey(const std::string& effect, const std::string& name, size_t component) {
    const auto hex = [](const std::string& value) {
        std::string out;
        for (unsigned char c : value) { out += "0123456789abcdef"[c >> 4]; out += "0123456789abcdef"[c & 15]; }
        return out;
    };
    return "makoShader_" + hex(effect) + "_" + hex(name) + "_" + std::to_string(component);
}
// Preserve legacy bare-name overrides, while allowing independent effects and vector components.
inline std::string shaderParameterValue(Config& config, const std::string& effect, const std::string& name, size_t component) {
    return config.getOption<std::string>(shaderParameterKey(effect, name, component), config.getOption<std::string>(name));
}
inline double shaderNumber(const reshadefx::constant& value, const reshadefx::type& type, size_t component = 0) {
    component = std::min<size_t>(component, 15);
    switch (type.base) {
        case reshadefx::type::t_float: return value.as_float[component];
        case reshadefx::type::t_uint: case reshadefx::type::t_bool: return value.as_uint[component];
        default: return value.as_int[component];
    }
}
inline uint32_t shaderParameterBits(const reshadefx::uniform_info& opt, const std::string& value) {
    uint32_t bits = opt.initializer_value.as_uint[0];
    if (value == "default") return bits; // Resolve defaults in the actual runtime extent/colour context.
    try {
        size_t consumed = 0;
        if (opt.type.base == reshadefx::type::t_bool) {
            auto boolean = value;
            std::transform(boolean.begin(), boolean.end(), boolean.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (boolean == "true" || boolean == "1") return 1;
            if (boolean == "false" || boolean == "0") return 0;
        } else if (opt.type.base == reshadefx::type::t_float) {
            float number = 0;
            std::istringstream input(value);
            input.imbue(std::locale::classic());
            if ((input >> number) && input.eof() && std::isfinite(number)) std::memcpy(&bits, &number, sizeof(bits));
        } else if (opt.type.base == reshadefx::type::t_uint) {
            const auto number = std::stoull(value, &consumed);
            if (value[0] != '-' && consumed == value.size() && number <= UINT32_MAX) bits = static_cast<uint32_t>(number);
        } else if (opt.type.base == reshadefx::type::t_int) {
            const auto number = std::stoll(value, &consumed);
            if (consumed == value.size() && number >= INT32_MIN && number <= INT32_MAX) bits = static_cast<uint32_t>(number);
        }
    } catch (const std::exception&) {} // Invalid external edits retain the declared default.
    return bits;
}
}
#endif
