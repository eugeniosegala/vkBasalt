#include "effect_dls.hpp"

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
    struct alignas(16) DlsSettings
    {
        float sharpness;
        float denoise;
        float padding[2];
    };

    DlsEffect::DlsEffect(LogicalDevice*       pLogicalDevice,
                         VkFormat             format,
                         VkExtent2D           imageExtent,
                         std::vector<VkImage> inputImages,
                         std::vector<VkImage> outputImages,
                         Config*              pConfig)
    {
        vertexCode   = full_screen_triangle_vert;
        fragmentCode = dls_frag;

        pVertexSpecInfo   = nullptr;
        pFragmentSpecInfo = nullptr;

        init(pLogicalDevice, format, imageExtent, inputImages, outputImages, pConfig, sizeof(DlsSettings));
        appliedConfigRevisions.assign(inputImages.size(), std::numeric_limits<uint64_t>::max());
    }
    void DlsEffect::updateEffect(uint32_t imageIndex)
    {
        if (imageIndex >= appliedConfigRevisions.size() || appliedConfigRevisions[imageIndex] == pConfig->revision())
            return;

        DlsSettings settings = {};
        settings.sharpness = pConfig->getOption<float>("dlsSharpness", 0.5f);
        settings.denoise = pConfig->getOption<float>("dlsDenoise", 0.17f);
        writeDynamicUniform(imageIndex, &settings, sizeof(settings));
        appliedConfigRevisions[imageIndex] = pConfig->revision();
    }
    DlsEffect::~DlsEffect()
    {
    }
} // namespace vkBasalt
