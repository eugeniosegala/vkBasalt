#include "logical_swapchain.hpp"

namespace vkBasalt
{
    void EffectGraph::destroy(LogicalDevice* pLogicalDevice)
    {
        if (!commandBuffers.empty())
        {
            pLogicalDevice->vkd.FreeCommandBuffers(pLogicalDevice->device,
                                                   pLogicalDevice->commandPool,
                                                   commandBuffers.size(),
                                                   commandBuffers.data());
            commandBuffers.clear();
        }
        effects.clear();
    }

    void LogicalSwapchain::destroy()
    {
        if (imageCount > 0)
        {
            activeEffectGraph.reset();
            for (auto& [key, graph] : effectGraphs)
                graph->destroy(pLogicalDevice);
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

            for (auto semaphore : semaphores)
                pLogicalDevice->vkd.DestroySemaphore(pLogicalDevice->device, semaphore, nullptr);
            Logger::debug("after DestroySemaphore");
        }
        intermediateImageSets.clear();
        intermediateImageMemories.clear();
        nonMutableOutputImages.clear();
        semaphores.clear();
        images.clear();
        fakeImages.clear();
        fakeImageMemory = VK_NULL_HANDLE;
        nonMutableOutputMemory = VK_NULL_HANDLE;
        imageCount = 0;
    }
} // namespace vkBasalt
