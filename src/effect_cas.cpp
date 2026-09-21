#include "effect_cas.hpp"

#include <cstring>
#include <limits>

#include "image_view.hpp"
#include "descriptor_set.hpp"
#include "buffer.hpp"
#include "renderpass.hpp"
#include "graphics_pipeline.hpp"
#include "framebuffer.hpp"
#include "shader.hpp"
#include "sampler.hpp"

#include "shader_sources.hpp"

namespace vkBasalt
{
    struct alignas(16) CasSettings
    {
        float sharpness;
        float padding[3];
    };

    CasEffect::CasEffect(LogicalDevice*       pLogicalDevice,
                         VkFormat             format,
                         VkExtent2D           imageExtent,
                         std::vector<VkImage> inputImages,
                         std::vector<VkImage> outputImages,
                         Config*              pConfig)
    {

        vertexCode   = full_screen_triangle_vert;
        fragmentCode = cas_frag;

        pVertexSpecInfo   = nullptr;
        pFragmentSpecInfo = nullptr;

        init(pLogicalDevice, format, imageExtent, inputImages, outputImages, pConfig, sizeof(CasSettings));
        appliedConfigRevisions.assign(inputImages.size(), std::numeric_limits<uint64_t>::max());
    }
    void CasEffect::updateEffect(uint32_t imageIndex)
    {
        if (imageIndex >= appliedConfigRevisions.size() || appliedConfigRevisions[imageIndex] == pConfig->revision())
            return;

        CasSettings settings = {};
        settings.sharpness = pConfig->getOption<float>("casSharpness", 0.4f);
        writeDynamicUniform(imageIndex, &settings, sizeof(settings));
        appliedConfigRevisions[imageIndex] = pConfig->revision();
    }
    CasEffect::~CasEffect()
    {
    }
} // namespace vkBasalt
