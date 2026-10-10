#include "logical_swapchain.hpp"

namespace vkBasalt
{
    EffectGraph::~EffectGraph() { if (device) destroy(device); }

    void EffectGraph::initializeDepthSubmissions(LogicalDevice* device, uint32_t count)
    {
        if (!usesDepthImage()) return;
        depthSubmissions.resize(count);
        const VkFenceCreateInfo info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        for (auto& slot : depthSubmissions)
            ASSERT_VULKAN(device->vkd.CreateFence(device->device, &info, nullptr, &slot.fence));
    }

    VkResult EffectGraph::waitForDepthSubmissions(LogicalDevice* device)
    {
        for (auto& slot : depthSubmissions) {
            if (!slot.pending) continue;
            const auto result = device->vkd.WaitForFences(device->device, 1, &slot.fence, VK_TRUE, UINT64_MAX);
            if (result != VK_SUCCESS) return result;
            slot.pending = false;
        }
        return VK_SUCCESS;
    }

    VkResult EffectGraph::prepareDepthSubmission(LogicalDevice* device, uint32_t index, VkFence& fence)
    {
        fence = VK_NULL_HANDLE;
        if (depthSubmissions.empty()) return VK_SUCCESS;
        auto& slot = depthSubmissions.at(index);
        if (slot.pending) {
            const auto result = device->vkd.WaitForFences(device->device, 1, &slot.fence, VK_TRUE, UINT64_MAX);
            if (result != VK_SUCCESS) return result;
            slot.pending = false;
        }
        const auto result = device->vkd.ResetFences(device->device, 1, &slot.fence);
        if (result == VK_SUCCESS) fence = slot.fence;
        return result;
    }

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
        for (const auto& slot : depthSubmissions)
            if (slot.fence != VK_NULL_HANDLE)
                pLogicalDevice->vkd.DestroyFence(pLogicalDevice->device, slot.fence, nullptr);
        depthSubmissions.clear();
        effects.clear();
        for (const auto& images : intermediateImageSets)
            for (const auto image : images)
                pLogicalDevice->vkd.DestroyImage(pLogicalDevice->device, image, nullptr);
        for (const auto memory : intermediateImageMemories)
            pLogicalDevice->vkd.FreeMemory(pLogicalDevice->device, memory, nullptr);
        intermediateImageSets.clear();
        intermediateImageMemories.clear();
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
        nonMutableOutputImages.clear();
        semaphores.clear();
        images.clear();
        fakeImages.clear();
        fakeImageMemory = VK_NULL_HANDLE;
        nonMutableOutputMemory = VK_NULL_HANDLE;
        imageCount = 0;
    }
} // namespace vkBasalt
