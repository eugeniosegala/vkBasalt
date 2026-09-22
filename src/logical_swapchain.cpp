#include "logical_swapchain.hpp"

namespace vkBasalt
{
    void LogicalSwapchain::destroy()
    {
        if (imageCount > 0)
        {
            activeEffectGraph.reset();
            for (auto& [key, graph] : effectGraphs)
            {
                graph->effects.clear();
                if (!graph->commandBuffers.empty())
                {
                    pLogicalDevice->vkd.FreeCommandBuffers(pLogicalDevice->device,
                                                           pLogicalDevice->commandPool,
                                                           graph->commandBuffers.size(),
                                                           graph->commandBuffers.data());
                }
            }
            effectGraphs.clear();
            Logger::debug("after free commandbuffer");

            for (const auto& images : intermediateImageSets)
            {
                for (const auto image : images)
                    pLogicalDevice->vkd.DestroyImage(pLogicalDevice->device, image, nullptr);
            }
            for (const auto memory : intermediateImageMemories)
                pLogicalDevice->vkd.FreeMemory(pLogicalDevice->device, memory, nullptr);

            for (const auto image : nonMutableOutputImages)
                pLogicalDevice->vkd.DestroyImage(pLogicalDevice->device, image, nullptr);
            if (nonMutableOutputMemory != VK_NULL_HANDLE)
                pLogicalDevice->vkd.FreeMemory(pLogicalDevice->device, nonMutableOutputMemory, nullptr);

            for (const auto image : fakeImages)
                pLogicalDevice->vkd.DestroyImage(pLogicalDevice->device, image, nullptr);
            if (fakeImageMemory != VK_NULL_HANDLE)
                pLogicalDevice->vkd.FreeMemory(pLogicalDevice->device, fakeImageMemory, nullptr);

            for (unsigned int i = 0; i < imageCount; i++)
            {
                pLogicalDevice->vkd.DestroySemaphore(pLogicalDevice->device, semaphores[i], nullptr);
            }
            Logger::debug("after DestroySemaphore");
        }
    }
} // namespace vkBasalt
