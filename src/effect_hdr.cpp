#include "effect_hdr.hpp"
#include "shader_sources.hpp"
namespace vkBasalt {
HdrEffect::HdrEffect(LogicalDevice* device, VkFormat inputFormat, VkFormat outputFormat,
        VkExtent2D extent, std::vector<VkImage> input, std::vector<VkImage> output,
        Config* config, int direction) {
    vertexCode = full_screen_triangle_vert;
    fragmentCode = hdr_frag;
    VkSpecializationMapEntry entry{0, 0, sizeof(direction)};
    VkSpecializationInfo info{1, &entry, sizeof(direction), &direction};
    pVertexSpecInfo = nullptr;
    pFragmentSpecInfo = &info;
    init(device, outputFormat, extent, input, output, config, 0, inputFormat);
    pFragmentSpecInfo = nullptr;
}
}
