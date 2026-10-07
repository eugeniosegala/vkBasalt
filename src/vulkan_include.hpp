#ifndef VULKAN_INCLUDE_HPP_INCLUDED
#define VULKAN_INCLUDE_HPP_INCLUDED

#define VK_NO_PROTOTYPES

#pragma GCC system_header
#include "vulkan/vulkan.h"
#include "vulkan/vk_layer.h"

#include <string>
#include <stdexcept>
#include <utility>

#include "logger.hpp"

namespace vkBasalt
{
    // Fail closed only inside candidate construction. Other layer entry points
    // retain their existing VkResult handling; exceptions never cross the ABI.
    inline thread_local bool checkedConstruction = false;
    struct CheckedConstruction
    {
        bool previous = checkedConstruction;
        CheckedConstruction() { checkedConstruction = true; }
        ~CheckedConstruction() { checkedConstruction = previous; }
    };
    template<typename F> struct ScopeExit
    {
        F cleanup;
        bool active = true;
        explicit ScopeExit(F fn) : cleanup(std::move(fn)) {}
        ~ScopeExit() { if (active) cleanup(); }
        void release() { active = false; }
    };
    struct VulkanError : std::runtime_error {
        VkResult result;
        VulkanError(VkResult value, const std::string& message) : std::runtime_error(message), result(value) {}
    };
    inline void checkVulkan(VkResult result, const char* file, int line)
    {
        if (result == VK_SUCCESS) return;
        const auto message = "ASSERT_VULKAN failed in " + std::string(file) + " : " + std::to_string(line) + "; " + std::to_string(result);
        Logger::err(message);
        if (checkedConstruction) throw VulkanError(result, message);
    }
}
#ifndef ASSERT_VULKAN
#define ASSERT_VULKAN(val) vkBasalt::checkVulkan((val), __FILE__, __LINE__);
#endif
namespace vkBasalt
{
    template<typename DispatchableType, typename SuperDispatchableType>
    inline void initializeDispatchTable(DispatchableType dispatchableObject, SuperDispatchableType source)
    {
        *reinterpret_cast<void**>(dispatchableObject) = *reinterpret_cast<void**>(source);
    }
} // namespace vkBasalt

#endif // VULKAN_INCLUDE_HPP_INCLUDED
