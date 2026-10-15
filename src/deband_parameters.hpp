#ifndef VKBASALT_DEBAND_PARAMETERS_HPP
#define VKBASALT_DEBAND_PARAMETERS_HPP
#include "shader_parameters.hpp"
#include <array>
namespace vkBasalt {
struct DebandParameter { const char* name; const char* label; float initial, minimum, maximum, step; bool integer; };
inline constexpr std::array<DebandParameter, 5> debandParameters{{
    {"debandAvgdiff", "Average difference", 3.4f, 0, 100, 0.1f, false},
    {"debandMaxdiff", "Maximum difference", 6.8f, 0, 100, 0.1f, false},
    {"debandMiddiff", "Middle difference", 3.3f, 0, 100, 0.1f, false},
    {"debandRange", "Range", 16, 1, 64, 1, false},
    {"debandIterations", "Iterations", 4, 1, 16, 1, true},
}};
inline float debandParameterValue(Config& config, const std::string& effect, size_t index) {
    const auto& p = debandParameters.at(index);
    reshadefx::uniform_info opt;
    opt.type.base = p.integer ? reshadefx::type::t_int : reshadefx::type::t_float;
    if (p.integer) opt.initializer_value.as_int[0] = static_cast<int32_t>(p.initial);
    else opt.initializer_value.as_float[0] = p.initial;
    const auto saved = shaderParameterValue(config, effect, p.name, 0);
    if (!saved.empty()) opt.initializer_value.as_uint[0] = shaderParameterBits(opt, saved);
    return static_cast<float>(shaderNumber(opt.initializer_value, opt.type));
}
}
#endif
