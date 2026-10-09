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
        const bool linear = color == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
        assert(swapchain.hdr() == hdr);
        assert(swapchain.effectFormat() == (hdr ? VK_FORMAT_R16G16B16A16_SFLOAT : swapchain.format));
        assert(swapchain.intermediateCount(empty) == 0);
        assert(!swapchain.needsHdrInputPass(empty) && !swapchain.needsHdrOutputPass(empty));
        assert(swapchain.intermediateCount(cas) == (linear ? 2 : 0));
        assert(swapchain.intermediateCount(custom) == (hdr ? 2 : 0));
        assert(swapchain.intermediateCount(chain) == (linear ? 3 : hdr ? 2 : 1));
        assert(swapchain.intermediateCount(reverse) == (linear ? 3 : hdr ? 2 : 1));
        assert(swapchain.intermediateCount(middle) == (hdr ? 4 : 2));
        assert(swapchain.needsHdrInputPass(chain) == hdr);
        assert(swapchain.needsHdrOutputPass(chain) == linear);
        assert(swapchain.needsHdrInputPass(reverse) == linear);
        assert(swapchain.needsHdrOutputPass(reverse) == hdr);
    }
}
