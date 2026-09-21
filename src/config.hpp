#ifndef CONFIG_HPP_INCLUDED
#define CONFIG_HPP_INCLUDED
#include <vector>
#include <fstream>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <cstdlib>
#include <cstdint>

#include "vulkan_include.hpp"

namespace vkBasalt
{
    class Config
    {
    public:
        Config();
        Config(const Config& other);

        bool configFileChanged() const;
        bool reloadIfChanged();
        const std::string& configFilePath() const;
        uint64_t revision() const;

        template<typename T>
        T getOption(const std::string& option, const T& defaultValue = {})
        {
            T result = defaultValue;
            parseOption(option, result);
            return result;
        }

    private:
        std::unordered_map<std::string, std::string> options;
        std::string selectedConfigFile;
        uint64_t configDevice = 0;
        uint64_t configInode = 0;
        uint64_t configSize = 0;
        int64_t configModifiedSeconds = 0;
        int64_t configModifiedNanoseconds = 0;
        bool configFileStateValid = false;
        uint64_t configRevision = 0;

        void readConfigLine(std::string line);
        void readConfigFile(std::ifstream& stream);
        void captureConfigFileState();

        void parseOption(const std::string& option, int32_t& result);
        void parseOption(const std::string& option, float& result);
        void parseOption(const std::string& option, bool& result);
        void parseOption(const std::string& option, std::string& result);
        void parseOption(const std::string& option, std::vector<std::string>& result);
    };
} // namespace vkBasalt

#endif // CONFIG_HPP_INCLUDED
