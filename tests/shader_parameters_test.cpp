#include "shader_parameters.hpp"
#include <cassert>
#include <fstream>
#include <filesystem>
#include <unistd.h>
int main() {
    using namespace vkBasalt;
    reshadefx::uniform_info opt;
    opt.type.base = reshadefx::type::t_float;
    opt.initializer_value.as_float[0] = 0.25f;
    auto decode = [&](const std::string& s) { auto v=opt.initializer_value; v.as_uint[0]=shaderParameterBits(opt,s); return v.as_float[0]; };
    assert(decode("0.75") == .75f && decode("-0.5") == -.5f);
    for (const auto bad : {"nan", "inf", "1junk", "1e99", ""}) assert(decode(bad) == .25f);
    assert(decode("default") == .25f);
    opt.initializer_value.as_float[0] = .8f;
    assert(decode("default") == .8f); // A later runtime context can have a different authored default.
    opt.type.base = reshadefx::type::t_bool;
    opt.initializer_value.as_uint[0] = 0;
    assert(shaderParameterBits(opt,"True") == 1 && shaderParameterBits(opt,"false") == 0);
    opt.type.base = reshadefx::type::t_uint;
    assert(shaderParameterBits(opt,"4294967295") == UINT32_MAX && shaderParameterBits(opt,"-1") == 0);
    const auto path = std::filesystem::temp_directory_path() / ("shader-parameters-" + std::to_string(getpid()));
    const auto a = shaderParameterKey("A", "Tint", 0), b = shaderParameterKey("B", "Tint", 0), v = shaderParameterKey("A", "Tint", 1);
    assert(a != b && a != v);
    std::ofstream(path) << "Tint = 0.5\n" << a << " = 0.8\n" << v << " = 0.2\n";
    setenv("VKBASALT_CONFIG_FILE", path.c_str(), 1);
    Config config;
    assert(shaderParameterValue(config,"A","Tint",0)=="0.8");
    assert(shaderParameterValue(config,"A","Tint",1)=="0.2");
    assert(shaderParameterValue(config,"B","Tint",0)=="0.5");
    std::filesystem::remove(path);
}
