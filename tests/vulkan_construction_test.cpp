#include "effect_reshade.hpp"
#include "effect_cas.hpp"
#include "effect_dls.hpp"
#include "image.hpp"
#include "logical_swapchain.hpp"
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include "buffer.hpp"
#include <cassert>
#include <iostream>
#include <set>
#include <tuple>
#include <type_traits>
#include <cstdint>
#include <cstring>

namespace vkBasalt
{
    VkBool32 VKAPI_CALL makoSetSwapchainHdrPrecisionV1(VkDevice, VkSwapchainKHR, VkBool32);
    VkBool32 VKAPI_CALL makoSetSwapchainColorSpaceV1(VkDevice, VkSwapchainKHR, VkColorSpaceKHR);
    extern std::shared_ptr<Config>                                               pConfig;
    std::shared_ptr<EffectGraph>                                                 buildEffectGraph(LogicalSwapchain*, const std::vector<std::string>&);
    std::shared_ptr<Effect> createConfiguredEffect(LogicalSwapchain*, const std::string&,
        const std::vector<VkImage>&, const std::vector<VkImage>&, Config*, VkFormat, VkFormat, VkFormat = VK_FORMAT_UNDEFINED);
    VkFormat requestedEffectFormat(LogicalSwapchain*);
    void                                                                         updateLiveEffectGraph(LogicalSwapchain*);
    extern std::unordered_map<void*, std::shared_ptr<LogicalDevice>>             deviceMap;
    extern std::unordered_map<VkSwapchainKHR, std::shared_ptr<LogicalSwapchain>> swapchainMap;
    VkResult vkBasalt_GetSwapchainImagesKHR(VkDevice, VkSwapchainKHR, uint32_t*, VkImage*);
} // namespace vkBasalt
namespace
{
    size_t             calls = 0, failAt = 0, nextHandle = 1;
    std::set<uint64_t> live;
    uintptr_t          commandSlots[64]{};
    size_t             commandSlot    = 0;
    VkResult           drainResult    = VK_SUCCESS;
    size_t drainCall = 0, failDrainAt = 0;
    VkFormatFeatureFlags missingFormatFeatures = 0;
    uintptr_t          deviceDispatch = 0;
    unsigned char      mapping[4096]{};
    std::vector<VkBool32> fragmentSpecializations;
    std::vector<VkFormat> imageViewFormats;
    size_t sampledOutputDependencies = 0;
    // Vulkan uses pointers for non-dispatchable handles on 64-bit and uint64_t
    // on 32-bit. Keep the fault tracker independent of that representation.
    template<typename T>
    T handleFromValue(uintptr_t value)
    {
        if constexpr (std::is_pointer_v<T>)
            return reinterpret_cast<T>(value);
        else
            return static_cast<T>(value);
    }
    template<typename T>
    uint64_t handleKey(T handle)
    {
        if constexpr (std::is_pointer_v<T>)
            return reinterpret_cast<uintptr_t>(handle);
        else
            return static_cast<uint64_t>(handle);
    }
    template<typename T>
    T allocate()
    {
        auto value = nextHandle++;
        live.insert(value);
        return handleFromValue<T>(value);
    }
    template<typename F>
    struct Mock;
    template<typename R, typename... A>
    struct Mock<R (*)(A...)>
    {
        static R create(A... args)
        {
            auto output = std::get<sizeof...(A) - 1>(std::tuple(args...));
            *output     = VK_NULL_HANDLE;
            if (++calls == failAt)
                return VK_ERROR_OUT_OF_DEVICE_MEMORY;
            *output = allocate<std::remove_pointer_t<decltype(output)>>();
            return VK_SUCCESS;
        }
        static R noop(A...)
        {
        }
        static R destroy(A... args)
        {
            auto handle = std::get<1>(std::tuple(args...));
            if (handle)
            {
                const auto removed = live.erase(handleKey(handle));
                assert(removed == 1); // catches duplicate destruction and foreign handles
            }
        }
    };
    vkBasalt::LogicalDevice driver()
    {
        vkBasalt::LogicalDevice d{};
        d.device = reinterpret_cast<VkDevice>(&deviceDispatch);
        d.vki.GetPhysicalDeviceFormatProperties = +[](VkPhysicalDevice, VkFormat, VkFormatProperties* props) {
            props->optimalTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
                VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
                VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
            props->optimalTilingFeatures &= ~missingFormatFeatures;
        };
#define CREATE(name)  d.vkd.name = &Mock<decltype(d.vkd.name)>::create
#define DESTROY(name) d.vkd.name = &Mock<decltype(d.vkd.name)>::destroy
        d.vkd.CreateImageView = +[](VkDevice device, const VkImageViewCreateInfo* info,
                                    const VkAllocationCallbacks* allocator, VkImageView* view) {
            imageViewFormats.push_back(info->format);
            return Mock<PFN_vkCreateImageView>::create(device, info, allocator, view);
        };
        CREATE(CreateFence);
        DESTROY(DestroyFence);
        CREATE(CreateShaderModule);
        CREATE(CreateDescriptorSetLayout);
        CREATE(CreateDescriptorPool);
        CREATE(CreatePipelineLayout);
        d.vkd.CreateRenderPass = +[](VkDevice device, const VkRenderPassCreateInfo* info,
                                    const VkAllocationCallbacks* allocator, VkRenderPass* renderPass) {
            for (uint32_t i = 0; i < info->dependencyCount; ++i) {
                const auto& dependency = info->pDependencies[i];
                if (dependency.srcSubpass == 0 && dependency.dstSubpass == VK_SUBPASS_EXTERNAL &&
                    (dependency.srcStageMask & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) &&
                    (dependency.srcAccessMask & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) &&
                    (dependency.dstStageMask & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) &&
                    (dependency.dstAccessMask & VK_ACCESS_SHADER_READ_BIT))
                    ++sampledOutputDependencies;
            }
            for (uint32_t i = 0; i < info->subpassCount; ++i)
                for (uint32_t j = 0; j < info->pSubpasses[i].colorAttachmentCount; ++j)
                {
                    const auto attachment = info->pSubpasses[i].pColorAttachments[j].attachment;
                    if (attachment == VK_ATTACHMENT_UNUSED ||
                        info->pAttachments[attachment].loadOp != VK_ATTACHMENT_LOAD_OP_LOAD)
                        continue;
                    bool readDependency = false;
                    for (uint32_t k = 0; k < info->dependencyCount; ++k)
                    {
                        const auto& dependency = info->pDependencies[k];
                        readDependency |= dependency.dstSubpass == i &&
                            (dependency.dstStageMask & VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) &&
                            (dependency.dstAccessMask & VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);
                    }
                    assert(readDependency);
                }
            return Mock<PFN_vkCreateRenderPass>::create(device, info, allocator, renderPass);
        };
        CREATE(CreateFramebuffer);
        CREATE(CreateSemaphore);
        CREATE(CreateSampler);
        CREATE(CreateBuffer);
        CREATE(AllocateMemory);
        CREATE(CreateImage);
        DESTROY(DestroyImageView);
        DESTROY(DestroyShaderModule);
        DESTROY(DestroyDescriptorSetLayout);
        DESTROY(DestroyDescriptorPool);
        DESTROY(DestroyPipelineLayout);
        DESTROY(DestroyRenderPass);
        DESTROY(DestroyFramebuffer);
        DESTROY(DestroySemaphore);
        DESTROY(DestroySampler);
        DESTROY(DestroyBuffer);
        DESTROY(FreeMemory);
        DESTROY(DestroyImage);
        DESTROY(DestroyPipeline);
        d.vkd.GetSwapchainImagesKHR = +[](VkDevice, VkSwapchainKHR, uint32_t* count, VkImage* images) {
            *count = 2;
            if (images)
            {
                images[0] = VK_NULL_HANDLE;
                images[1] = VK_NULL_HANDLE;
            }
            return VK_SUCCESS;
        };
        d.vkd.CreateGraphicsPipelines =
            +[](VkDevice, VkPipelineCache, uint32_t count, const VkGraphicsPipelineCreateInfo* info, const VkAllocationCallbacks*, VkPipeline* pipelines) {
                assert(count == 1);
                for (uint32_t i = 0; i < info->stageCount; ++i)
                {
                    const auto& stage = info->pStages[i];
                    if (stage.stage != VK_SHADER_STAGE_FRAGMENT_BIT || !stage.pSpecializationInfo) continue;
                    const auto& spec = *stage.pSpecializationInfo;
                    if (spec.mapEntryCount == 1 && spec.pMapEntries[0].constantID == 0 &&
                        spec.pMapEntries[0].size == sizeof(VkBool32))
                    {
                        VkBool32 value;
                        assert(spec.pMapEntries[0].offset + sizeof(value) <= spec.dataSize);
                        std::memcpy(&value, static_cast<const char*>(spec.pData) + spec.pMapEntries[0].offset, sizeof(value));
                        fragmentSpecializations.push_back(value);
                    }
                }
                // Vulkan permits partial pipeline results when a batch returns an error.
                *pipelines = allocate<VkPipeline>();
                return ++calls == failAt ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS;
            };
        d.vkd.GetBufferMemoryRequirements = +[](VkDevice, VkBuffer buffer, VkMemoryRequirements* req) {
            assert(buffer);
            *req = {4096, 256, 1};
        };
        d.vkd.GetImageMemoryRequirements = +[](VkDevice, VkImage image, VkMemoryRequirements* req) {
            assert(image);
            *req = {4096, 256, 1};
        };
        d.vki.GetPhysicalDeviceMemoryProperties = +[](VkPhysicalDevice, VkPhysicalDeviceMemoryProperties* properties) {
            *properties                 = {};
            properties->memoryTypeCount = 1;
            properties->memoryTypes[0].propertyFlags =
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        };
        d.vkd.BindBufferMemory = +[](VkDevice, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize) {
            assert(buffer && memory);
            return ++calls == failAt ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS;
        };
        d.vkd.BindImageMemory = +[](VkDevice, VkImage image, VkDeviceMemory memory, VkDeviceSize) {
            assert(image && memory);
            return ++calls == failAt ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS;
        };
        d.vkd.MapMemory = +[](VkDevice, VkDeviceMemory memory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void** output) {
            assert(memory);
            *output = nullptr;
            if (++calls == failAt)
                return VK_ERROR_OUT_OF_DEVICE_MEMORY;
            *output = mapping;
            return VK_SUCCESS;
        };
        d.vkd.UnmapMemory            = +[](VkDevice, VkDeviceMemory memory) { assert(memory); };
        d.vkd.AllocateDescriptorSets = +[](VkDevice, const VkDescriptorSetAllocateInfo* info, VkDescriptorSet* sets) {
            if (++calls == failAt)
                return VK_ERROR_OUT_OF_DEVICE_MEMORY;
            // Sets are owned by their pool and do not require independent freeing.
            for (uint32_t i = 0; i < info->descriptorSetCount; ++i)
                sets[i] = handleFromValue<VkDescriptorSet>(1);
            return VK_SUCCESS;
        };
        d.vkd.AllocateCommandBuffers = +[](VkDevice, const VkCommandBufferAllocateInfo* info, VkCommandBuffer* buffers) {
            if (++calls == failAt)
                return VK_ERROR_OUT_OF_DEVICE_MEMORY;
            for (uint32_t i = 0; i < info->commandBufferCount; ++i)
            {
                assert(commandSlot < 64);
                buffers[i] = reinterpret_cast<VkCommandBuffer>(&commandSlots[commandSlot++]);
                live.insert(reinterpret_cast<uintptr_t>(buffers[i]));
            }
            return VK_SUCCESS;
        };
        d.vkd.FreeCommandBuffers = +[](VkDevice, VkCommandPool, uint32_t count, const VkCommandBuffer* buffers) {
            for (uint32_t i = 0; i < count; ++i)
                assert(live.erase(reinterpret_cast<uintptr_t>(buffers[i])) == 1);
        };
        d.vkd.BeginCommandBuffer =
            +[](VkCommandBuffer, const VkCommandBufferBeginInfo*) { return ++calls == failAt ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS; };
        d.vkd.EndCommandBuffer = +[](VkCommandBuffer) { return ++calls == failAt ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS; };
        d.vkd.QueueWaitIdle    = +[](VkQueue) { return ++drainCall == failDrainAt ? VK_ERROR_OUT_OF_DEVICE_MEMORY : drainResult; };
        d.vkd.DeviceWaitIdle   = +[](VkDevice) { return drainResult; };
        d.vkd.QueueSubmit      = +[](VkQueue, uint32_t, const VkSubmitInfo*, VkFence) { return VK_SUCCESS; };
#define NOOP(name) d.vkd.name = &Mock<decltype(d.vkd.name)>::noop
        NOOP(CmdPipelineBarrier);
        NOOP(CmdBeginRenderPass);
        NOOP(CmdEndRenderPass);
        NOOP(CmdCopyBufferToImage);
        NOOP(CmdBindDescriptorSets);
        NOOP(CmdBindPipeline);
        NOOP(CmdDraw);
        NOOP(CmdCopyImage);
        d.vkd.UpdateDescriptorSets = +[](VkDevice, uint32_t, const VkWriteDescriptorSet*, uint32_t, const VkCopyDescriptorSet*) {};
        return d;
    }
} // namespace

int main()
{
    // Independently declared MAKO v1 ABI: explicit colour, correct owner and
    // pre-exposure lifetime are required, including handle reuse.
    {
        using Handoff = VkBool32 (VKAPI_PTR *)(VkDevice, VkSwapchainKHR, VkColorSpaceKHR);
        static_assert(std::is_same_v<decltype(&vkBasalt::makoSetSwapchainColorSpaceV1), Handoff>);
        const Handoff handoff = &vkBasalt::makoSetSwapchainColorSpaceV1;
        auto device = std::make_shared<vkBasalt::LogicalDevice>();
        device->device = handleFromValue<VkDevice>(19);
        auto swapchain = std::make_shared<vkBasalt::LogicalSwapchain>();
        swapchain->pLogicalDevice = device.get();
        swapchain->format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        const auto handle = handleFromValue<VkSwapchainKHR>(20);
        assert(!handoff(device->device, handle, VK_COLOR_SPACE_HDR10_ST2084_EXT));
        vkBasalt::swapchainMap[handle] = swapchain;
        assert(!handoff(VK_NULL_HANDLE, handle, VK_COLOR_SPACE_HDR10_ST2084_EXT));
        assert(!handoff(device->device, handle, VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT));
        assert(!handoff(device->device, handle, VK_COLOR_SPACE_HDR10_HLG_EXT));
        assert(handoff(device->device, handle, VK_COLOR_SPACE_HDR10_ST2084_EXT));
        assert(swapchain->hdr());
        const auto precision = &vkBasalt::makoSetSwapchainHdrPrecisionV1;
        assert(precision(device->device, handle, VK_TRUE));
        assert(swapchain->hdrReducedPrecision && swapchain->hdrPrecisionRevision == 1);
        assert(precision(device->device, handle, VK_TRUE) && swapchain->hdrPrecisionRevision == 1);
        assert(!precision(device->device, handle, 2));
        assert(!precision(VK_NULL_HANDLE, handle, VK_FALSE));
        assert(precision(device->device, handle, VK_FALSE) && !swapchain->hdrReducedPrecision);

        swapchain->fakeImages.push_back(handleFromValue<VkImage>(21));
        assert(!handoff(device->device, handle, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR));
        swapchain->fakeImages.clear();
        vkBasalt::swapchainMap.erase(handle);
        assert(!handoff(device->device, handle, VK_COLOR_SPACE_HDR10_ST2084_EXT));
        swapchain = std::make_shared<vkBasalt::LogicalSwapchain>();
        swapchain->pLogicalDevice = device.get();
        swapchain->format = VK_FORMAT_R16G16B16A16_SFLOAT;
        vkBasalt::swapchainMap[handle] = swapchain;
        assert(!swapchain->hdr());
        assert(handoff(device->device, handle, VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT));
        assert(handoff(device->device, handle, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR));
        assert(!swapchain->hdr());
        vkBasalt::swapchainMap.clear();
    }

    setenv("VKBASALT_CONFIG_FILE", "/dev/null", 1);
    setenv("VKBASALT_LOG_LEVEL", "none", 1);
    vkBasalt::Config              config;
    auto                          d = driver();
    vkBasalt::CheckedConstruction checked;
    auto                          exercise = [&](auto build) {
        size_t total = 0;
        for (size_t failure = 0; failure <= total; ++failure)
        {
            calls         = 0;
            failAt        = failure;
            bool rejected = false;
            try
            {
                build();
            }
            catch (const std::runtime_error&)
            {
                rejected = true;
            }
            if (failure == 0)
                total = calls;
            else
                assert(rejected);
            if (!live.empty())
            {
                std::cerr << "failure=" << failure << " calls=" << calls << " live=";
                for (auto value : live)
                    std::cerr << value << ",";
                std::cerr << "\n";
            }
            assert(live.empty());
        }
    };
    exercise([&] {
        vkBasalt::PreparedReshadeModule module;
        module.module.spirv = {0x07230203};
        reshadefx::technique_info technique;
        reshadefx::pass_info      pass;
        pass.vs_entry_point = "VS";
        pass.ps_entry_point = "PS";
        technique.passes    = {pass};
        module.module.techniques.push_back(technique);
        vkBasalt::ReshadeEffect effect(&d,
                                       VK_FORMAT_B8G8R8A8_UNORM,
                                       {320, 240},
                                       {VK_NULL_HANDLE, VK_NULL_HANDLE},
                                       {VK_NULL_HANDLE, VK_NULL_HANDLE},
                                       &config,
                                       "CustomProbe",
                                       std::move(module));
    });
    exercise([&] { vkBasalt::CasEffect effect(&d, VK_FORMAT_B8G8R8A8_UNORM, {320, 240}, {VK_NULL_HANDLE}, {VK_NULL_HANDLE}, &config); });
    exercise([&] { vkBasalt::DlsEffect effect(&d, VK_FORMAT_B8G8R8A8_UNORM, {320, 240}, {VK_NULL_HANDLE}, {VK_NULL_HANDLE}, &config); });
    exercise([&] {
        VkDeviceMemory memory = VK_NULL_HANDLE;
        auto           images =
            vkBasalt::createImages(&d, 3, {32, 32, 1}, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memory);
        for (auto image : images)
            d.vkd.DestroyImage(d.device, image, nullptr);
        d.vkd.FreeMemory(d.device, memory, nullptr);
    });
    // Confirmed colour space, not bit depth, selects the HDR specialization.
    vkBasalt::pConfig = std::make_shared<vkBasalt::Config>();
    failAt = 0;
    for (auto color : {VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, VK_COLOR_SPACE_HDR10_ST2084_EXT,
                      VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT})
    {
        vkBasalt::LogicalSwapchain swapchain{};
        swapchain.pLogicalDevice = &d;
        swapchain.colorSpace = color;
        swapchain.format = color == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT
            ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        swapchain.imageExtent = {320, 240};
        for (const auto& name : {"cas", "dls"})
        {
            fragmentSpecializations.clear();
            {
                auto effect = vkBasalt::createConfiguredEffect(&swapchain, name,
                    {VK_NULL_HANDLE}, {VK_NULL_HANDLE}, &config,
                    swapchain.effectFormat(), swapchain.effectFormat());
                assert(fragmentSpecializations == std::vector<VkBool32>{swapchain.hdr() ? VK_TRUE : VK_FALSE});
            }
            assert(live.empty());
        }
    }
    exercise([&] { vkBasalt::CasEffect effect(&d, VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        {320, 240}, {VK_NULL_HANDLE}, {VK_NULL_HANDLE}, &config, VK_FORMAT_R16G16B16A16_SFLOAT, true); });
    exercise([&] { vkBasalt::DlsEffect effect(&d, VK_FORMAT_R16G16B16A16_SFLOAT,
        {320, 240}, {VK_NULL_HANDLE}, {VK_NULL_HANDLE}, &config, true); });

    // Every allocation/recording failure must preserve the real previous graph.
    const auto root = std::filesystem::temp_directory_path() / ("vkbasalt-fault-" + std::to_string(getpid()));
    std::filesystem::create_directories(root);
    std::ofstream(root / "Tone.fx") << R"(
void VS(uint id : SV_VertexID, out float4 pos : SV_Position) { pos = float4(0,0,0,1); }
texture Input : COLOR;
sampler InputSampler { Texture = Input; };
float4 PS(float4 pos : SV_Position) : SV_Target { return tex2D(InputSampler, pos.xy) * float4(.98,.99,1,1); }
technique Tone {
    pass { VertexShader = VS; PixelShader = PS; }
    pass { VertexShader = VS; PixelShader = PS; }
    pass { VertexShader = VS; PixelShader = PS; }
}
)";
    size_t total = 0;
    for (const std::string selection : {"CustomTone:cas", "CustomTone"})
    {
        std::ofstream(root / "vkBasalt.conf") << "effects = " << selection << "\nCustomTone = \"" << (root / "Tone.fx").string() << "\"\n";
        setenv("VKBASALT_CONFIG_FILE", (root / "vkBasalt.conf").c_str(), 1);
        vkBasalt::pConfig = std::make_shared<vkBasalt::Config>();
        for (const bool reduced : {false, true})
        for (auto color : {VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, VK_COLOR_SPACE_HDR10_ST2084_EXT,
                           VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT})
        {
            total = 0;
            for (size_t failure = 0; failure <= total; ++failure)
            {
                commandSlot = 0;
                calls       = 0;
                failAt      = 0;
                vkBasalt::LogicalSwapchain swapchain{};
                swapchain.pLogicalDevice                       = &d;
                swapchain.colorSpace                           = color;
                swapchain.format                               = color == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT
                    ? VK_FORMAT_R16G16B16A16_SFLOAT : color == VK_COLOR_SPACE_HDR10_ST2084_EXT
                    ? VK_FORMAT_A2B10G10R10_UNORM_PACK32 : VK_FORMAT_B8G8R8A8_UNORM;
                swapchain.imageExtent                          = {320, 240};
                swapchain.swapchainCreateInfo.imageFormat      = swapchain.format;
                swapchain.swapchainCreateInfo.imageExtent      = swapchain.imageExtent;
                swapchain.swapchainCreateInfo.imageArrayLayers = 1;
                swapchain.imageCount                           = 2;
                swapchain.images                               = {VK_NULL_HANDLE, VK_NULL_HANDLE};
                swapchain.fakeImages                           = swapchain.images;
                d.supportsMutableFormat                        = true;
                auto previous                                  = vkBasalt::buildEffectGraph(&swapchain, {"cas"});
                swapchain.activeEffectGraph                    = previous;
                swapchain.effectGraphs["cas"]                  = previous;
                swapchain.effectGraphs[""]                     = vkBasalt::buildEffectGraph(&swapchain, {});
                swapchain.hdrReducedPrecision = reduced;
                swapchain.hdrPrecisionRevision = reduced;
                const auto originalResources                   = live;
                calls                                          = 0;
                failAt                                         = failure;
                swapchain.effectSelectionRevision              = UINT64_MAX;
                imageViewFormats.clear();
                sampledOutputDependencies = 0;
                vkBasalt::updateLiveEffectGraph(&swapchain);
                if (failure == 0)
                {
                    total = calls;
                    assert(swapchain.activeEffectGraph != previous);
                    assert(sampledOutputDependencies >= 3);
                    if (color == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
                        // Two views for each of two direct HDR10 input images,
                        // then float output views; no graph-boundary copy.
                        assert(imageViewFormats.size() > 4);
                        for (size_t i = 0; i < 4; ++i)
                            assert(imageViewFormats[i] == swapchain.format);
                        assert(imageViewFormats[4] == swapchain.effectFormat(reduced));
                        const bool directCompact = reduced && selection == "CustomTone";
                        assert(swapchain.activeEffectGraph->effects.size() == (directCompact ? 1u : 2u));
                        assert(swapchain.activeEffectGraph->intermediateImageSets.size() == (directCompact ? 0u : 1u));
                    }
                }
                else
                {
                    assert(swapchain.activeEffectGraph == previous);
                    assert(live == originalResources);
                    assert(swapchain.activeEffectGraph->intermediateImageSets.size() == swapchain.intermediateCount({"cas"}));
                }
                if (failure == 0 && reduced && swapchain.hdr()) {
                    // No config revision or effect selection change: precision alone
                    // must atomically rebuild, and an identical request is inert.
                    const auto compact = swapchain.activeEffectGraph;
                    assert(compact->workingFormat == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
                    swapchain.hdrReducedPrecision = false;
                    ++swapchain.hdrPrecisionRevision;
                    vkBasalt::updateLiveEffectGraph(&swapchain);
                    assert(swapchain.activeEffectGraph != compact);
                    assert(swapchain.activeEffectGraph->workingFormat == VK_FORMAT_R16G16B16A16_SFLOAT);
                    const auto restored = swapchain.activeEffectGraph;
                    const auto count = calls;
                    vkBasalt::updateLiveEffectGraph(&swapchain);
                    assert(swapchain.activeEffectGraph == restored && calls == count);
                    // A compact format lacking a required feature must keep the
                    // existing graph, including inherited STORAGE image usage.
                    swapchain.hdrReducedPrecision = true;
                    for (auto missing : {VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT,
                                         VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT}) {
                        missingFormatFeatures = missing;
                        swapchain.swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
                        assert(vkBasalt::requestedEffectFormat(&swapchain) == VK_FORMAT_R16G16B16A16_SFLOAT);
                    }
                    missingFormatFeatures = 0;
                    // Multiple failed final drains at one config revision must
                    // retain distinct candidates until a later successful drain.
                    for (int attempt = 0; attempt < 2; ++attempt) {
                        swapchain.hdrPrecisionRevision += 2; // off/on between presents
                        drainCall = 0;
                        failDrainAt = 2;
                        const auto held = live;
                        vkBasalt::updateLiveEffectGraph(&swapchain);
                        assert(swapchain.activeEffectGraph == restored);
                        assert(swapchain.effectGraphs.size() == size_t(3 + attempt));
                        for (const auto handle : held) assert(live.count(handle));
                        assert(live.size() > held.size());
                    }
                    failDrainAt = 0;
                    swapchain.hdrPrecisionRevision += 2;
                    vkBasalt::updateLiveEffectGraph(&swapchain);
                    assert(swapchain.activeEffectGraph != restored);
                    assert(swapchain.activeEffectGraph->workingFormat == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
                    assert(swapchain.effectGraphs.size() == 2);
                }
                swapchain.destroy();
                assert(live.empty());
            }
        }
    }
    // Startup allocation errors return a VkResult; no exception or null graph
    // may escape through the Vulkan ABI. A subsequent call can start cleanly.
    total = 0;
    for (size_t failure = 0; failure <= total; ++failure)
    {
        commandSlot                                     = 0;
        calls                                           = 0;
        failAt                                          = failure;
        auto device                                     = std::make_shared<vkBasalt::LogicalDevice>(d);
        vkBasalt::deviceMap[nullptr]                    = device;
        auto swapchain                                  = std::make_shared<vkBasalt::LogicalSwapchain>();
        swapchain->pLogicalDevice                       = device.get();
        swapchain->format                               = VK_FORMAT_B8G8R8A8_UNORM;
        swapchain->imageExtent                          = {320, 240};
        swapchain->swapchainCreateInfo.imageFormat      = swapchain->format;
        swapchain->swapchainCreateInfo.imageExtent      = swapchain->imageExtent;
        swapchain->swapchainCreateInfo.imageArrayLayers = 1;
        const auto handle                               = handleFromValue<VkSwapchainKHR>(1);
        vkBasalt::swapchainMap[handle]                  = swapchain;
        uint32_t   count                                = 2;
        VkImage    images[2]{};
        const auto result = vkBasalt::vkBasalt_GetSwapchainImagesKHR(device->device, handle, &count, images);
        if (failure == 0)
            total = calls;
        if (result == VK_SUCCESS)
        {
            assert(swapchain->activeEffectGraph && images[0] && images[1]);
        }
        else
        {
            assert(result == VK_ERROR_OUT_OF_DEVICE_MEMORY);
            assert(swapchain->fakeImages.empty() && swapchain->imageCount == 0);
        }
        swapchain->destroy();
        assert(live.empty());
        vkBasalt::swapchainMap.clear();
        vkBasalt::deviceMap.clear();
    }
    vkBasalt::pConfig.reset();
    std::ofstream(root / "Texture.fx") << R"(
texture Tex < source = "Texture.tga"; > { Width = 1; Height = 1; Format = RGBA8; };
sampler Samp { Texture = Tex; };
void VS(uint id : SV_VertexID, out float4 pos : SV_Position) { pos = float4(0,0,0,1); }
float4 PS(float4 pos : SV_Position) : SV_Target { return tex2D(Samp, pos.xy); }
technique Tone { pass { VertexShader = VS; PixelShader = PS; } }
)";
    const char    tga[] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 32, 8, 10, 20, 30, 40};
    std::ofstream image(root / "Texture.tga", std::ios::binary);
    image.write(tga, sizeof(tga));
    image.close();
    std::ofstream(root / "vkBasalt.conf") << "CustomTex = \"" << (root / "Texture.fx").string() << "\"\nreshadeTexturePath = " << root.string()
                                          << "\n";
    vkBasalt::Config textureConfig;
    for (auto error : {VK_ERROR_OUT_OF_DEVICE_MEMORY, VK_ERROR_DEVICE_LOST})
    {
        commandSlot   = 0;
        failAt        = 0;
        calls         = 0;
        drainResult   = error;
        bool rejected = false;
        try
        {
            auto                    module = vkBasalt::compileReshadeModule(textureConfig, "CustomTex", {320, 240}, false);
            vkBasalt::ReshadeEffect effect(
                &d, VK_FORMAT_B8G8R8A8_UNORM, {320, 240}, {VK_NULL_HANDLE}, {VK_NULL_HANDLE}, &textureConfig, "CustomTex", std::move(module));
        }
        catch (const std::runtime_error&)
        {
            rejected = true;
        }
        assert(rejected);
        if (error == VK_ERROR_DEVICE_LOST)
        {
            assert(live.empty() && d.pendingUploads.empty() && d.pendingUploadEffects.empty());
        }
        else
        {
            // Both waits failed without confirmed device loss: retain every
            // texture/upload handle until a later successful drain.
            assert(!live.empty() && d.pendingUploads.size() == 1 && d.pendingUploadEffects.size() == 1);
            drainResult = VK_SUCCESS;
            assert(d.vkd.QueueWaitIdle(d.queue) == VK_SUCCESS);
            d.retirePendingUploads();
            assert(live.empty() && !d.uploadQueueUncertain);
        }
    }
    std::filesystem::remove_all(root);
}
