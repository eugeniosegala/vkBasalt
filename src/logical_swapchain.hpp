#ifndef LOGICAL_SWAPCHAIN_HPP_INCLUDED
#define LOGICAL_SWAPCHAIN_HPP_INCLUDED
#include <vector>
#include <fstream>
#include <string>
#include <iostream>
#include <vector>
#include <memory>
#include <map>

#include "effect.hpp"
#include "config.hpp"

#include "vulkan_include.hpp"

#include "logical_device.hpp"

namespace vkBasalt
{
    struct EffectGraph
    {
        std::shared_ptr<Config>             config;
        std::vector<std::string>            effectNames;
        std::vector<std::shared_ptr<Effect>> effects;
        std::vector<VkCommandBuffer>         commandBuffers;

        void destroy(LogicalDevice* pLogicalDevice);
    };

    // for each swapchain, we have the Images and the other stuff we need to execute the compute shader
    struct LogicalSwapchain
    {
        LogicalDevice*                       pLogicalDevice;
        VkSwapchainCreateInfoKHR             swapchainCreateInfo;
        VkExtent2D                           imageExtent;
        VkFormat                             format;
        VkColorSpaceKHR                       colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        bool hdr() const { return colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT ||
            colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT; }
        VkFormat effectFormat() const { return hdr() ? VK_FORMAT_R16G16B16A16_SFLOAT : format; }
        // CAS can sample and render different normalized formats directly.
        // Keep RGBA16F between effects; scRGB still needs both transfer passes.
        bool needsHdrInputPass(const std::vector<std::string>& effects) const {
            return hdr() && !effects.empty() &&
                (colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT || effects.front() != "cas");
        }
        bool needsHdrOutputPass(const std::vector<std::string>& effects) const {
            return hdr() && !effects.empty() &&
                (colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT || effects.back() != "cas");
        }
        size_t intermediateCount(const std::vector<std::string>& effects) const {
            return effects.empty() ? 0 : effects.size() - 1 +
                needsHdrInputPass(effects) + needsHdrOutputPass(effects);
        }

        uint32_t                             imageCount;
        std::vector<VkImage>                 images;
        std::vector<VkImage>                 fakeImages;
        std::vector<std::vector<VkImage>>    intermediateImageSets;
        std::vector<VkDeviceMemory>          intermediateImageMemories;
        std::vector<VkImage>                 nonMutableOutputImages;
        std::vector<VkSemaphore>             semaphores;
        std::map<std::string, std::shared_ptr<EffectGraph>> effectGraphs;
        std::shared_ptr<EffectGraph>          activeEffectGraph;
        VkDeviceMemory                       fakeImageMemory = VK_NULL_HANDLE;
        VkDeviceMemory                       nonMutableOutputMemory = VK_NULL_HANDLE;
        uint64_t                             effectSelectionRevision = 0;

        void destroy();
    };
} // namespace vkBasalt

#endif // LOGICAL_SWAPCHAIN_HPP_INCLUDED
