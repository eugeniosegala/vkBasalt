#ifndef IMAGE_VIEW_HPP_INCLUDED
#define IMAGE_VIEW_HPP_INCLUDED
#include <vector>
#include <fstream>
#include <string>
#include <iostream>
#include <vector>
#include <memory>

#include "vulkan_include.hpp"

#include "logical_device.hpp"

namespace vkBasalt
{
    // Effect inputs and outputs are swapchain-derived images that can carry
    // the application's usage, including storage. Their sRGB views must not
    // inherit usage the view format lacks, so they declare only what effects
    // use; every such image is created with both bits.
    constexpr VkImageUsageFlags effectImageViewUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // A non-zero usage restricts each view through VkImageViewUsageCreateInfo.
    std::vector<VkImageView> createImageViews(LogicalDevice*       pLogicalDevice,
                                              VkFormat             format,
                                              std::vector<VkImage> images,
                                              VkImageViewType      viewType   = VK_IMAGE_VIEW_TYPE_2D,
                                              VkImageAspectFlags   aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                              uint32_t             mipLevels  = 1,
                                              VkImageUsageFlags    usage      = 0);
}

#endif // IMAGE_VIEW_HPP_INCLUDED
