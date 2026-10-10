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
        EffectGraph() = default;
        EffectGraph(const EffectGraph&) = delete;
        EffectGraph& operator=(const EffectGraph&) = delete;
        std::shared_ptr<Config>             config;
        std::vector<std::string>            effectNames;
        std::vector<std::shared_ptr<Effect>> effects;
        std::vector<VkCommandBuffer>         commandBuffers;

        LogicalDevice* device = nullptr;
        VkFormat workingFormat = VK_FORMAT_UNDEFINED;
        VkImage depthImage = VK_NULL_HANDLE;
        bool usesDepthImage() const {
            for (const auto& effect : effects)
                if (effect->usesDepthImage()) return true;
            return false;
        }
        std::vector<std::vector<VkImage>> intermediateImageSets;
        std::vector<VkDeviceMemory> intermediateImageMemories;
        struct DepthSubmission {
            VkFence fence = VK_NULL_HANDLE;
            bool pending = false;
        };
        std::vector<DepthSubmission> depthSubmissions;
        void initializeDepthSubmissions(LogicalDevice*, uint32_t imageCount);
        VkResult prepareDepthSubmission(LogicalDevice*, uint32_t index, VkFence& fence);
        void depthSubmitted(uint32_t index) {
            if (!depthSubmissions.empty()) depthSubmissions.at(index).pending = true;
        }
        VkResult waitForDepthSubmissions(LogicalDevice*);
        ~EffectGraph();
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
        VkFormat effectFormat(bool reducedPrecision = false) const {
            return !hdr() ? format : reducedPrecision
                ? VK_FORMAT_A2B10G10R10_UNORM_PACK32 : VK_FORMAT_R16G16B16A16_SFLOAT;
        }
        bool hdrReducedPrecision = false;
        uint64_t hdrPrecisionRevision = 0;
        uint64_t effectPrecisionRevision = 0;
        // CAS/DLS and ReShade can sample HDR10 directly. Other native effects
        // still require matching input/output formats. Private PQ intermediates
        // follow the selected precision; scRGB needs both transfer passes.
        bool needsHdrInputPass(const std::vector<std::string>& effects) const {
            return hdr() && !effects.empty() &&
                (colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT ||
                 effects.front() == "fxaa" || effects.front() == "smaa" ||
                 effects.front() == "deband" || effects.front() == "makoDeband" || effects.front() == "lut");
        }
        bool needsHdrOutputPass(const std::vector<std::string>& effects,
                                VkFormat workingFormat = VK_FORMAT_UNDEFINED) const {
            if (!hdr() || effects.empty()) return false;
            if (colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT) return true;
            if (effects.back() == "cas" || effects.back() == "dls") return false;
            // Use the graph's validated format, not the requested precision:
            // unsupported compact storage must retain its conversion boundary.
            // Identical PQ formats need no final full-resolution copy. Existing
            // native/ReShade effects can already render into that exact format.
            if (workingFormat == VK_FORMAT_UNDEFINED) workingFormat = effectFormat();
            return workingFormat != format;
        }
        size_t intermediateCount(const std::vector<std::string>& effects,
                                 VkFormat workingFormat = VK_FORMAT_UNDEFINED) const {
            return effects.empty() ? 0 : effects.size() - 1 +
                needsHdrInputPass(effects) + needsHdrOutputPass(effects, workingFormat);
        }

        uint32_t                             imageCount;
        std::vector<VkImage>                 images;
        std::vector<VkImage>                 fakeImages;
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
