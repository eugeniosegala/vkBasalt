#pragma once

#include "vulkan_include.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace vkBasalt
{
    // Normal single-window presents need no heap storage. Preserve arbitrary
    // application wait lists and multi-swapchain presentation with a fallback.
    class PresentScratch
    {
    public:
        PresentScratch(size_t waitCount, size_t swapchainCount)
        {
            if (waitCount > inlineWaitStages.size())
                extraWaitStages.assign(waitCount, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            else
                std::fill_n(inlineWaitStages.begin(), waitCount, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            if (swapchainCount > inlineSemaphores.size())
                extraSemaphores.resize(swapchainCount);
        }

        const VkPipelineStageFlags* waitStages() const
        {
            return extraWaitStages.empty() ? inlineWaitStages.data() : extraWaitStages.data();
        }
        VkSemaphore* semaphores()
        {
            return extraSemaphores.empty() ? inlineSemaphores.data() : extraSemaphores.data();
        }

    private:
        std::array<VkPipelineStageFlags, 4> inlineWaitStages{};
        std::array<VkSemaphore, 4> inlineSemaphores{};
        std::vector<VkPipelineStageFlags> extraWaitStages;
        std::vector<VkSemaphore> extraSemaphores;
    };
}
