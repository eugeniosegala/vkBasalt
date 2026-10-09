#pragma once
#include "effect_simple.hpp"
namespace vkBasalt {
// Convert at the effect-graph boundary; all managed effects work on bounded PQ
// code values in floating-point images. Zero is a format-preserving copy.
class HdrEffect final : public SimpleEffect {
public:
    HdrEffect(LogicalDevice* device, VkFormat inputFormat, VkFormat outputFormat,
              VkExtent2D extent, std::vector<VkImage> input, std::vector<VkImage> output,
              Config* config, int direction);
};
}
