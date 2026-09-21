#include "config.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>

namespace
{
    void writeConfigAtomically(const std::filesystem::path& path, const std::string& content)
    {
        const auto temporary = path.string() + ".new";
        {
            std::ofstream output(temporary);
            output << content;
        }
        std::filesystem::rename(temporary, path);
    }
}

int main()
{
    const auto directory = std::filesystem::temp_directory_path() /
                           ("vkbasalt-config-reload-test-" + std::to_string(getpid()));
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const auto configPath = directory / "vkBasalt.conf";

    writeConfigAtomically(configPath, "effects = \ncasSharpness = 0.40\n");
    setenv("VKBASALT_CONFIG_FILE", configPath.c_str(), 1);
    setenv("VKBASALT_LOG_LEVEL", "none", 1);

    vkBasalt::Config config;
    assert(config.configFilePath() == configPath);
    assert(config.getOption<std::vector<std::string>>("effects", {"cas"}).empty());
    assert(std::abs(config.getOption<float>("casSharpness") - 0.40f) < 0.001f);
    assert(config.revision() == 0);
    assert(!config.reloadIfChanged());

    writeConfigAtomically(configPath, "effects = cas\ncasSharpness = 0.85\n");
    assert(config.configFileChanged());
    assert(config.reloadIfChanged());
    assert(config.getOption<std::vector<std::string>>("effects") == std::vector<std::string>({"cas"}));
    assert(std::abs(config.getOption<float>("casSharpness") - 0.85f) < 0.001f);
    assert(config.revision() == 1);
    assert(!config.reloadIfChanged());

    std::filesystem::remove_all(directory);
    return 0;
}
