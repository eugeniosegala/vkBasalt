#include "descriptor_set.hpp"
#include <cassert>

int main()
{
    // No Vulkan dispatch is needed for procedural shaders without samplers.
    assert(vkBasalt::allocateAndWriteImageSamplerDescriptorSets(
        nullptr, VK_NULL_HANDLE, VK_NULL_HANDLE, {}, {}).empty());
}
