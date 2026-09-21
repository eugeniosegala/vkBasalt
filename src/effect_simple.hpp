#ifndef EFFECT_SIMPLE_HPP_INCLUDED
#define EFFECT_SIMPLE_HPP_INCLUDED
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

#include "logical_device.hpp"

namespace vkBasalt
{
    class SimpleEffect : public Effect
    {
    public:
        SimpleEffect();
        void virtual applyEffect(uint32_t imageIndex, VkCommandBuffer commandBuffer) override;
        virtual ~SimpleEffect();

    protected:
        LogicalDevice*               pLogicalDevice;
        std::vector<VkImage>         inputImages;
        std::vector<VkImage>         outputImages;
        std::vector<VkImageView>     inputImageViews;
        std::vector<VkImageView>     outputImageViews;
        std::vector<VkDescriptorSet> imageDescriptorSets;
        std::vector<VkFramebuffer>   framebuffers;
        VkDescriptorSetLayout        imageSamplerDescriptorSetLayout;
        VkDescriptorPool             descriptorPool;
        VkShaderModule               vertexModule;
        VkShaderModule               fragmentModule;
        VkRenderPass                 renderPass;
        VkPipelineLayout             pipelineLayout;
        VkPipeline                   graphicsPipeline;
        VkExtent2D                   imageExtent;
        VkFormat                     format;
        VkSampler                    sampler;
        Config*                      pConfig;
        VkDescriptorSetLayout        dynamicUniformDescriptorSetLayout = VK_NULL_HANDLE;
        std::vector<VkBuffer>         dynamicUniformBuffers;
        std::vector<VkDeviceMemory>   dynamicUniformMemories;
        std::vector<void*>            dynamicUniformMappings;
        std::vector<VkDescriptorSet>  dynamicUniformDescriptorSets;
        VkDeviceSize                  dynamicUniformSize = 0;
        std::vector<uint32_t>        vertexCode;
        std::vector<uint32_t>        fragmentCode;
        VkSpecializationInfo*        pVertexSpecInfo;
        VkSpecializationInfo*        pFragmentSpecInfo;

        // subclasses can put DescriptorSets in here, but the first one will be the input image descriptorSet
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

        void init(LogicalDevice*       pLogicalDevice,
                  VkFormat             format,
                  VkExtent2D           imageExtent,
                  std::vector<VkImage> inputImages,
                  std::vector<VkImage> outputImages,
                  Config*              pConfig,
                  VkDeviceSize         dynamicUniformSize = 0);
        void writeDynamicUniform(uint32_t imageIndex, const void* data, VkDeviceSize size);
    };
} // namespace vkBasalt

#endif // EFFECT_SIMPLE_HPP_INCLUDED
