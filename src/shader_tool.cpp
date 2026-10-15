// Uses the exact runtime compiler; never executes shader code or creates a Vulkan device.
#include "reshade_module.hpp"
#include "shader_parameters.hpp"
#include "deband_parameters.hpp"
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <sys/resource.h>
using namespace vkBasalt;
static std::string json(const std::string& text) {
    std::ostringstream out; out << '"';
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c) << std::dec;
        else out << c;
    }
    out << '"'; return out.str();
}
static const reshadefx::annotation* annotation(const reshadefx::uniform_info& uniform, const char* name) {
    for (const auto& item : uniform.annotations) if (item.name == name) return &item;
    return nullptr;
}
static std::string text(const reshadefx::uniform_info& uniform, const char* name) {
    const auto* item = annotation(uniform, name); return item ? item->value.string_data : "";
}
static void range(const reshadefx::uniform_info& uniform, const char* name) {
    const auto* item = annotation(uniform, name);
    if (!item) { std::cout << "null"; return; }
    const auto index = item->type.rows * item->type.cols > 1 ? uniform.offset : 0;
    const double value = shaderNumber(item->value, item->type, index);
    if (std::isfinite(value)) std::cout << value; else std::cout << "null";
}
int main(int argc, char** argv) {
    // Bound source discovery independently of the game and UI processes.
    const rlimit memoryLimit{512u * 1024u * 1024u, 512u * 1024u * 1024u};
    const rlimit cpuLimit{20, 20};
    if (setrlimit(RLIMIT_AS, &memoryLimit) || setrlimit(RLIMIT_CPU, &cpuLimit)) {
        std::cerr << "Unable to bound shader discovery resources\n"; return 1;
    }
    if (argc != 2) { std::cerr << "Usage: mako-shader-tool CONFIG\n"; return 2; }
    if (!std::filesystem::is_regular_file(argv[1])) { std::cerr << "Shader configuration is unavailable\n"; return 1; }
    setenv("VKBASALT_CONFIG_FILE", argv[1], 1);
    Config config;
    if (config.configFilePath() != argv[1]) return 1;
    std::cout << std::setprecision(9) << "{\"schema\":1,\"effects\":[";
    bool firstEffect = true;
    for (const auto& effect : config.getOption<std::vector<std::string>>("effects")) {
        if (effect == "makoDeband" || effect == "deband") {
            if (!firstEffect) std::cout << ',';
            firstEffect = false;
            std::cout << "{\"id\":" << json(effect) << ",\"name\":\"Deband\",\"path\":\"\",\"error\":null,\"parameters\":[";
            for (size_t i = 0; i < debandParameters.size(); ++i) {
                if (i) std::cout << ',';
                const auto& p = debandParameters[i];
                std::cout << "{\"key\":" << json(shaderParameterKey(effect, p.name, 0)) << ",\"label\":" << json(p.label)
                          << ",\"description\":\"\",\"category\":\"\",\"type\":" << json(p.integer ? "int" : "float")
                          << ",\"default\":" << p.initial << ",\"value\":" << debandParameterValue(config, effect, i)
                          << ",\"min\":" << p.minimum << ",\"max\":" << p.maximum << ",\"step\":" << p.step << ",\"items\":[]}";
            }
            std::cout << "]}";
            continue;
        }
        const auto path = config.getOption<std::string>(effect);
        auto extension = std::filesystem::path(path).extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension != ".fx") continue;
        if (!firstEffect) std::cout << ',';
        firstEffect = false;
        std::cout << "{\"id\":" << json(effect) << ",\"name\":" << json(std::filesystem::path(path).stem().string()) << ",\"path\":" << json(path);
        try {
            const auto prepared = compileReshadeModule(config, effect, {1920,1080}, false, false);
            std::unordered_map<std::string, size_t> counts, components;
            for (const auto& opt : prepared.module.spec_constants) ++counts[opt.name];
            std::cout << ",\"parameters\":[";
            bool first = true;
            for (const auto& opt : prepared.module.spec_constants) {
                const size_t component = components[opt.name]++;
                const auto* hidden = annotation(opt, "hidden");
                if (!hidden) hidden = annotation(opt, "ui_hidden");
                if (annotation(opt, "source") || (hidden && shaderNumber(hidden->value, hidden->type) != 0)) continue;
                if (opt.type.base != reshadefx::type::t_bool && opt.type.base != reshadefx::type::t_float && opt.type.base != reshadefx::type::t_int && opt.type.base != reshadefx::type::t_uint) continue;
                if (!first) std::cout << ',';
                first = false;
                auto label = text(opt, "ui_label"); if (label.empty()) label = opt.name;
                if (counts[opt.name] > 1) label += " [" + std::to_string(component + 1) + "]";
                auto value = opt.initializer_value;
                const auto saved = shaderParameterValue(config, effect, opt.name, component);
                if (!saved.empty()) value.as_uint[0] = shaderParameterBits(opt, saved);
                const double initial = shaderNumber(opt.initializer_value, opt.type), current = shaderNumber(value, opt.type);
                std::cout << "{\"key\":" << json(shaderParameterKey(effect, opt.name, component))
                          << ",\"label\":" << json(label) << ",\"description\":" << json(text(opt, "ui_tooltip"))
                          << ",\"category\":" << json(text(opt, "ui_category")) << ",\"type\":"
                          << json(opt.type.base == reshadefx::type::t_bool ? "bool" : opt.type.base == reshadefx::type::t_float ? "float" : opt.type.base == reshadefx::type::t_uint ? "uint" : "int")
                          << ",\"default\":" << (std::isfinite(initial) ? initial : 0)
                          << ",\"value\":" << (std::isfinite(current) ? current : 0)
                          << ",\"min\":"; range(opt, "ui_min"); std::cout << ",\"max\":"; range(opt, "ui_max");
                std::cout << ",\"step\":"; range(opt, "ui_step");
                std::cout << ",\"items\":[";
                std::istringstream items(text(opt,"ui_items")); std::string item; bool firstItem = true;
                while (std::getline(items, item, '\0')) { if (item.empty()) continue; if (!firstItem) std::cout << ','; firstItem = false; std::cout << json(item); }
                std::cout << "]}";
            }
            std::cout << "],\"error\":null";
        } catch (const std::exception& e) { std::cout << ",\"parameters\":[],\"error\":" << json(e.what()); }
        std::cout << '}';
    }
    std::cout << "]}\n";
}
