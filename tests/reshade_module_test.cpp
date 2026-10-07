#include "reshade_module.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <unistd.h>

int main()
{
    const auto directory = std::filesystem::temp_directory_path() /
        ("vkbasalt-live-compile-" + std::to_string(getpid()));
    std::filesystem::create_directories(directory);
    const auto shader = directory / "custom tone.fx";
    const auto configPath = directory / "vkBasalt.conf";
    const std::string source = R"(
void VS(uint id : SV_VertexID, out float4 pos : SV_Position) {
    pos = float4((id == 2 ? 3.0 : -1.0), (id == 1 ? 3.0 : -1.0), 0.0, 1.0);
}
float4 PS(float4 pos : SV_Position) : SV_Target { return float4(0.2, 0.8, 0.3, 1.0); }
technique Tone { pass { VertexShader = VS; PixelShader = PS; } }
)";
    std::ofstream(shader) << source;
    std::ofstream(configPath) << "effects = CustomTone\nCustomTone = \"" << shader.string() << "\"\n";
    setenv("VKBASALT_CONFIG_FILE", configPath.c_str(), 1);
    vkBasalt::Config config;
    const auto valid = vkBasalt::compileReshadeModule(config, "CustomTone", {1280, 720}, false);
    assert(!valid.module.spirv.empty() && valid.module.techniques.size() == 1);
    auto rejected = [&] {
        try { vkBasalt::compileReshadeModule(config, "CustomTone", {1280, 720}, false); }
        catch (const std::runtime_error&) { return true; }
        return false;
    };
    std::ofstream(shader) << "this is not a shader";
    assert(rejected());
    std::ofstream(shader) << "#include \"Missing.fxh\"\n" << source;
    assert(rejected());
    std::ofstream(shader) << "float4 unused() { return 0; }";
    assert(rejected());
    std::filesystem::remove(shader);
    assert(rejected());
    std::ofstream(shader) << "#warning \"compatible warning\"\n" << source;
    const auto recovered = vkBasalt::compileReshadeModule(config, "CustomTone", {1280, 720}, false);
    assert(!recovered.module.spirv.empty() && recovered.module.techniques.size() == 1);
    const std::string textured = R"(
texture Tex < source = "missing.png"; > { Width = 1; Height = 1; Format = RGBA8; };
sampler Samp { Texture = Tex; };
void VS(uint id : SV_VertexID, out float4 pos : SV_Position) { pos = float4(0,0,0,1); }
float4 PS(float4 pos : SV_Position) : SV_Target { return tex2D(Samp, pos.xy); }
technique Tone { pass { VertexShader = VS; PixelShader = PS; } }
)";
    std::ofstream(shader) << textured;
    assert(rejected());
    std::ofstream(configPath) << "CustomTone = \"" << shader.string() << "\"\nreshadeTexturePath = " << directory.string() << "\n";
    vkBasalt::Config texturedConfig;
    std::ofstream(directory / "missing.png") << "invalid image bytes";
    try { vkBasalt::compileReshadeModule(texturedConfig, "CustomTone", {1280, 720}, false); assert(false); }
    catch (const std::runtime_error&) {}
    const char tga[] = {0,0,2,0,0,0,0,0,0,0,0,0,1,0,1,0,32,8,10,20,30,40};
    std::ofstream image(directory / "missing.png", std::ios::binary);
    image.write(tga, sizeof(tga));
    image.close();
    const auto texturedModule = vkBasalt::compileReshadeModule(texturedConfig, "CustomTone", {1280, 720}, false);
    assert(texturedModule.textures.size() == 1);
    assert(texturedModule.textures.begin()->second.pixels.size() == 4);
    std::ofstream(shader) << std::string(textured).replace(textured.find("Width = 1"), 9, "Width = 65536");
    try { vkBasalt::compileReshadeModule(texturedConfig, "CustomTone", {1280,720}, false); assert(false); }
    catch (const std::runtime_error&) {}
    std::filesystem::remove_all(directory);
}
