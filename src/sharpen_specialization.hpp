#pragma once
#include "vulkan_include.hpp"
#include <cstddef>

namespace vkBasalt {
// Immutable per-graph sampling footprint. Native/SDR pipelines keep exactly
// their original integer-offset instructions; no new uniform update is needed.
struct SharpenSpecialization {
    SharpenSpecialization(const SharpenSpecialization&) = delete;
    SharpenSpecialization& operator=(const SharpenSpecialization&) = delete;
    struct Values { VkBool32 hdr; float width; float height; } values;
    VkSpecializationMapEntry entries[3] = {
        {0, offsetof(Values, hdr), sizeof(VkBool32)},
        {1, offsetof(Values, width), sizeof(float)},
        {2, offsetof(Values, height), sizeof(float)}
    };
    VkSpecializationInfo info{3, entries, sizeof(values), &values};
    SharpenSpecialization(bool hdr, VkExtent2D output, VkExtent2D source)
        : values{VkBool32(hdr), 0.0f, 0.0f} {
        if (hdr && source.width && source.height &&
            source.width <= output.width && source.height <= output.height &&
            (source.width < output.width || source.height < output.height)) {
            values.width = 1.0f / float(source.width);
            values.height = 1.0f / float(source.height);
        }
    }
};
}
