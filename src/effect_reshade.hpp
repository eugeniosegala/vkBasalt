#ifndef EFFECT_RESHADE_HPP_INCLUDED
#define EFFECT_RESHADE_HPP_INCLUDED
#include <vector>
#include <fstream>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>

#include "vulkan_include.hpp"

#include "effect.hpp"
#include "config.hpp"
#include "reshade_uniforms.hpp"
#include "reshade_module.hpp"

#include "logical_device.hpp"

#include "reshade/effect_parser.hpp"
#include "reshade/effect_codegen.hpp"
#include "reshade/effect_preprocessor.hpp"

namespace vkBasalt
{
    class ReshadeEffect : public Effect
    {
    public:
        ReshadeEffect(LogicalDevice*       pLogicalDevice,
                      VkFormat             format,
                      VkExtent2D           imageExtent,
                      std::vector<VkImage> inputImages,
                      std::vector<VkImage> outputImages,
                      Config*              pConfig,
                      std::string          effectName,
                      PreparedReshadeModule compiledModule,
                      VkFormat inputFormat = VK_FORMAT_UNDEFINED);
        void virtual applyEffect(uint32_t imageIndex, VkCommandBuffer commandBuffer) override;
        void virtual updateEffect(uint32_t imageIndex) override;
        void virtual useDepthImage(VkImageView depthImageView) override;
        virtual ~ReshadeEffect();

    private:
        ReshadeEffect() = default;
        ReshadeEffect& operator=(ReshadeEffect&&) = default;
        void destroyResources();
        LogicalDevice*           pLogicalDevice;
        std::vector<VkImage>     inputImages;
        std::vector<VkImage>     outputImages;
        std::vector<VkImageView> inputImageViewsSRGB;
        std::vector<VkImageView> inputImageViewsUNORM;
        std::vector<VkImageView> outputImageViewsSRGB;
        std::vector<VkImageView> outputImageViewsUNORM;

        std::unordered_map<std::string, std::vector<VkImage>>     textureImages;
        std::unordered_map<std::string, std::vector<VkImageView>> textureImageViewsUNORM;
        std::unordered_map<std::string, std::vector<VkImageView>> textureImageViewsSRGB;
        std::unordered_map<std::string, std::vector<VkImageView>> renderImageViewsSRGB;
        std::unordered_map<std::string, std::vector<VkImageView>> renderImageViewsUNORM;

        std::unordered_map<std::string, VkFormat>   textureFormatsUNORM;
        std::unordered_map<std::string, VkFormat>   textureFormatsSRGB;
        std::unordered_map<std::string, uint32_t>   textureMipLevels;
        std::unordered_map<std::string, VkExtent3D> textureExtents;

        std::vector<VkDescriptorSet> inputDescriptorSets;
        std::vector<VkDescriptorSet> outputDescriptorSets;
        std::vector<VkDescriptorSet> backBufferDescriptorSets;

        std::vector<std::vector<VkFramebuffer>> framebuffers;

        VkDescriptorSetLayout                 uniformDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout                 imageSamplerDescriptorSetLayout = VK_NULL_HANDLE;
        VkShaderModule                        shaderModule = VK_NULL_HANDLE;
        VkDescriptorPool                      descriptorPool = VK_NULL_HANDLE;
        std::vector<VkRenderPass>             renderPasses;
        std::vector<std::vector<std::string>> renderTargets;
        std::vector<VkRenderPassBeginInfo>    renderPassBeginInfos;
        VkPipelineLayout                      pipelineLayout = VK_NULL_HANDLE;
        std::vector<VkPipeline>               graphicsPipelines;
        std::vector<bool>                     switchSamplers;
        VkExtent2D                            imageExtent;
        std::vector<VkSampler>                samplers;
        Config*                               pConfig;
        std::string                           effectName;
        reshadefx::module                     module;
        std::vector<VkDeviceMemory>           textureMemory;

        VkFormat    inputOutputFormatUNORM;
        VkFormat    inputOutputFormatSRGB;
        VkFormat    stencilFormat = VK_FORMAT_UNDEFINED;
        VkImage     stencilImage = VK_NULL_HANDLE;
        VkImageView stencilImageView = VK_NULL_HANDLE;
        // how often the shader writes to the reshade back buffer
        // we need to flip the "backbuffer" after each write if there is a next one
        int                      outputWrites = 0;
        std::vector<VkImage>     backBufferImages;
        std::vector<VkImageView> backBufferImageViewsUNORM;
        std::vector<VkImageView> backBufferImageViewsSRGB;
        VkBuffer                 stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory           stagingBufferMemory = VK_NULL_HANDLE;
        void*                    stagingBufferMapping = nullptr;
        uint32_t                 bufferSize = 0;
        VkDescriptorSet          bufferDescriptorSet = VK_NULL_HANDLE;

        std::vector<std::shared_ptr<ReshadeUniform>> uniforms;

        VkFormat      convertReshadeFormat(reshadefx::texture_format texFormat);
        VkCompareOp   convertReshadeCompareOp(reshadefx::pass_stencil_func compareOp);
        VkStencilOp   convertReshadeStencilOp(reshadefx::pass_stencil_op stencilOp);
        VkBlendOp     convertReshadeBlendOp(reshadefx::pass_blend_op blendOp);
        VkBlendFactor convertReshadeBlendFactor(reshadefx::pass_blend_func blendFactor);
    };
} // namespace vkBasalt

#endif // EFFECT_RESHADE_HPP_INCLUDED
