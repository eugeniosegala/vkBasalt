#include "logical_swapchain.hpp"
#include <cassert>
int main() {
    vkBasalt::LogicalSwapchain swapchain{};
    const std::vector<std::string> empty{}, cas{"cas"}, custom{"custom"},
        chain{"custom", "cas"}, reverse{"cas", "custom"}, middle{"custom", "cas", "custom"};
    for (auto color : {VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, VK_COLOR_SPACE_HDR10_ST2084_EXT,
                       VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT}) {
        swapchain.colorSpace = color;
        swapchain.format = color == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT
            ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        const bool hdr = color != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        const std::vector<std::string> dls{"dls"}, both{"cas", "dls"};
        const bool linear = color == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
        assert(swapchain.hdr() == hdr);
        assert(swapchain.effectFormat(true) == (hdr
            ? VK_FORMAT_A2B10G10R10_UNORM_PACK32 : swapchain.format));
        assert(swapchain.effectFormat() == (hdr ? VK_FORMAT_R16G16B16A16_SFLOAT : swapchain.format));
        assert(swapchain.intermediateCount(empty) == 0);
        assert(!swapchain.needsHdrInputPass(empty) && !swapchain.needsHdrOutputPass(empty));
        assert(swapchain.intermediateCount(cas) == (linear ? 2 : 0));
        assert(swapchain.intermediateCount(dls) == (linear ? 2 : 0));
        assert(swapchain.intermediateCount(both) == (linear ? 3 : 1));
        assert(swapchain.intermediateCount(custom) == (linear ? 2 : hdr ? 1 : 0));
        assert(swapchain.intermediateCount(chain) == (linear ? 3 : 1));
        assert(swapchain.intermediateCount(reverse) == (linear ? 3 : hdr ? 2 : 1));
        assert(swapchain.intermediateCount(middle) == (linear ? 4 : hdr ? 3 : 2));
        assert(swapchain.needsHdrInputPass(chain) == linear);
        assert(swapchain.needsHdrOutputPass(chain) == linear);
        assert(swapchain.needsHdrInputPass(reverse) == linear);
        assert(swapchain.needsHdrOutputPass(reverse) == hdr);
        for (const auto& native : {"fxaa", "smaa", "deband", "makoDeband", "lut"})
            assert(swapchain.needsHdrInputPass({native, "cas"}) == hdr);
        for (const auto& customName : {"custom", "makoVibrance", "makoLevelsPlus", "makoClarity"})
            assert(swapchain.needsHdrInputPass({customName, "cas"}) == linear);
    }
    // Only an actually matching PQ graph may elide its output copy. A compact
    // request that falls back to FP16 and alternate channel order retain it.
    swapchain.colorSpace = VK_COLOR_SPACE_HDR10_ST2084_EXT;
    swapchain.format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    swapchain.hdrReducedPrecision = true;
    for (const auto& last : {"custom", "makoLevelsPlus", "makoClarity", "fxaa", "smaa", "lut"}) {
        assert(!swapchain.needsHdrOutputPass({last}, VK_FORMAT_A2B10G10R10_UNORM_PACK32));
        assert(swapchain.needsHdrOutputPass({last}, VK_FORMAT_R16G16B16A16_SFLOAT));
    }
    assert(swapchain.intermediateCount({"makoLevelsPlus"}, swapchain.format) == 0);
    assert(swapchain.intermediateCount({"makoLevelsPlus", "makoClarity"}, swapchain.format) == 1);
    assert(swapchain.intermediateCount({"fxaa"}, swapchain.format) == 1);
    swapchain.format = VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    assert(swapchain.needsHdrOutputPass(custom, VK_FORMAT_A2B10G10R10_UNORM_PACK32));
    swapchain.colorSpace = VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
    swapchain.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    assert(swapchain.needsHdrOutputPass(custom, swapchain.format));
    swapchain.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    assert(!swapchain.needsHdrOutputPass(custom, swapchain.format));

}
