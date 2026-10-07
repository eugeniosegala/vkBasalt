#ifndef LOGICAL_DEVICE_HPP_INCLUDED
#define LOGICAL_DEVICE_HPP_INCLUDED
#include <vector>
#include <memory>
#include <fstream>
#include <string>
#include <iostream>
#include <vector>

#include "vulkan_include.hpp"
#include "vkdispatch.hpp"

namespace vkBasalt
{
    class Effect;
    struct LogicalDevice
    {
        DeviceDispatch           vkd;
        InstanceDispatch         vki;
        VkDevice                 device;
        VkPhysicalDevice         physicalDevice;
        VkInstance               instance;
        VkQueue                  queue;
        uint32_t                 queueFamilyIndex;
        VkCommandPool            commandPool;
        bool                     supportsMutableFormat;
        std::vector<VkImage>     depthImages;
        std::vector<VkFormat>    depthFormats;
        std::vector<VkImageView> depthImageViews;
        struct PendingUpload {
            VkCommandBuffer commandBuffer;
            VkBuffer buffer;
            VkDeviceMemory memory;
        };
        std::vector<PendingUpload> pendingUploads;
        std::vector<std::shared_ptr<Effect>> pendingUploadEffects;
        bool uploadQueueUncertain = false;
        bool destroyed = false;

        void retirePendingUploads() {
            for (const auto& upload : pendingUploads) {
                if (upload.commandBuffer != VK_NULL_HANDLE)
                    vkd.FreeCommandBuffers(device, commandPool, 1, &upload.commandBuffer);
                vkd.DestroyBuffer(device, upload.buffer, nullptr);
                vkd.FreeMemory(device, upload.memory, nullptr);
            }
            pendingUploads.clear();
            uploadQueueUncertain = false;
            pendingUploadEffects.clear();
        }
    };
} // namespace vkBasalt

#endif // LOGICAL_DEVICE_HPP_INCLUDED
