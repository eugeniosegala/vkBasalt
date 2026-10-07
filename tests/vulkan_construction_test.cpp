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

namespace vkBasalt
{
    extern std::shared_ptr<Config>                                               pConfig;
    std::shared_ptr<EffectGraph>                                                 buildEffectGraph(LogicalSwapchain*, const std::vector<std::string>&);
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
    uintptr_t          deviceDispatch = 0;
    unsigned char      mapping[4096]{};
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
#define CREATE(name)  d.vkd.name = &Mock<decltype(d.vkd.name)>::create
#define DESTROY(name) d.vkd.name = &Mock<decltype(d.vkd.name)>::destroy
        CREATE(CreateImageView);
        CREATE(CreateShaderModule);
        CREATE(CreateDescriptorSetLayout);
        CREATE(CreateDescriptorPool);
        CREATE(CreatePipelineLayout);
        CREATE(CreateRenderPass);
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
            +[](VkDevice, VkPipelineCache, uint32_t count, const VkGraphicsPipelineCreateInfo*, const VkAllocationCallbacks*, VkPipeline* pipelines) {
                assert(count == 1);
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
        d.vkd.QueueWaitIdle    = +[](VkQueue) { return drainResult; };
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
    // Every allocation/recording failure must preserve the real previous graph.
    const auto root = std::filesystem::temp_directory_path() / ("vkbasalt-fault-" + std::to_string(getpid()));
    std::filesystem::create_directories(root);
    std::ofstream(root / "Tone.fx") << R"(
void VS(uint id : SV_VertexID, out float4 pos : SV_Position) { pos = float4(0,0,0,1); }
float4 PS(float4 pos : SV_Position) : SV_Target { return float4(1,0,0,1); }
technique Tone { pass { VertexShader = VS; PixelShader = PS; } }
)";
    std::ofstream(root / "vkBasalt.conf") << "effects = CustomTone:cas\nCustomTone = \"" << (root / "Tone.fx").string() << "\"\n";
    setenv("VKBASALT_CONFIG_FILE", (root / "vkBasalt.conf").c_str(), 1);
    vkBasalt::pConfig = std::make_shared<vkBasalt::Config>();
    size_t total      = 0;
    for (size_t failure = 0; failure <= total; ++failure)
    {
        commandSlot = 0;
        calls       = 0;
        failAt      = 0;
        vkBasalt::LogicalSwapchain swapchain{};
        swapchain.pLogicalDevice                       = &d;
        swapchain.format                               = VK_FORMAT_B8G8R8A8_UNORM;
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
        const auto originalResources                   = live;
        calls                                          = 0;
        failAt                                         = failure;
        swapchain.effectSelectionRevision              = UINT64_MAX;
        vkBasalt::updateLiveEffectGraph(&swapchain);
        if (failure == 0)
        {
            total = calls;
            assert(swapchain.activeEffectGraph != previous);
        }
        else
        {
            assert(swapchain.activeEffectGraph == previous);
            assert(live == originalResources);
            assert(swapchain.intermediateImageSets.empty());
        }
        swapchain.destroy();
        assert(live.empty());
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
