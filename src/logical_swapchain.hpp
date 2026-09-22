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

#include "vulkan_include.hpp"

#include "logical_device.hpp"

namespace vkBasalt
{
    struct EffectGraph
    {
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
