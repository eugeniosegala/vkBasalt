#include "vulkan_version.hpp"
#include "vulkan_include.hpp"

#include <mutex>
#include <map>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <string>
#include <memory>
#include <cstring>
#include <chrono>
#include <cstdlib>

#include "util.hpp"
#include "keyboard_input.hpp"

#include "logical_device.hpp"
#include "logical_swapchain.hpp"
#include "live_effects.hpp"
#include "toggle_key_poll.hpp"

#include "image_view.hpp"
#include "sampler.hpp"
#include "framebuffer.hpp"
#include "descriptor_set.hpp"
#include "shader.hpp"
#include "graphics_pipeline.hpp"
#include "command_buffer.hpp"
#include "buffer.hpp"
#include "config.hpp"
#include "fake_swapchain.hpp"
#include "renderpass.hpp"
#include "format.hpp"
#include "effect_hdr.hpp"
#include "logger.hpp"

#include "effect.hpp"
#include "effect_fxaa.hpp"
#include "effect_cas.hpp"
#include "effect_dls.hpp"
#include "effect_smaa.hpp"
#include "effect_deband.hpp"
#include "effect_lut.hpp"
#include "effect_reshade.hpp"
#include "reshade_module.hpp"
#include "effect_transfer.hpp"

#define VKBASALT_NAME "VK_LAYER_VKBASALT_post_processing"

#if defined(__GNUC__) && __GNUC__ >= 4
#define VK_BASALT_EXPORT __attribute__((visibility("default")))
#else
#error "Unsupported platform!"
#endif

namespace vkBasalt
{
    std::shared_ptr<Config> pConfig = nullptr;

    // layer book-keeping information, to store dispatch tables by key
    std::unordered_map<void*, InstanceDispatch>                           instanceDispatchMap;
    std::unordered_map<void*, VkInstance>                                 instanceMap;
    std::unordered_map<void*, uint32_t>                                   instanceVersionMap;
    std::unordered_map<void*, std::shared_ptr<LogicalDevice>>             deviceMap;
    std::unordered_map<VkSwapchainKHR, std::shared_ptr<LogicalSwapchain>> swapchainMap;

    std::mutex globalLock;
#ifdef _GCC_
    using scoped_lock __attribute__((unused)) = std::lock_guard<std::mutex>;
#else
    using scoped_lock = std::lock_guard<std::mutex>;
#endif

    template<typename DispatchableType>
    void* GetKey(DispatchableType inst)
    {
        return *(void**) inst;
    }

    bool liveConfigReloadEnabled()
    {
        static const bool enabled = []() {
            const char* value = std::getenv("VKBASALT_CONFIG_RELOAD");
            return value != nullptr && (std::strcmp(value, "1") == 0 || std::strcmp(value, "true") == 0
                                        || std::strcmp(value, "True") == 0);
        }();
        return enabled;
    }

    void reloadConfigIfChanged()
    {
        if (!liveConfigReloadEnabled() || pConfig == nullptr || pConfig->configFilePath().empty())
            return;

        static auto nextCheck = std::chrono::steady_clock::time_point::min();
        const auto now = std::chrono::steady_clock::now();
        if (now < nextCheck)
            return;
        nextCheck = now + std::chrono::milliseconds(250);

        if (pConfig->reloadIfChanged())
            Logger::info("reloaded live options from " + pConfig->configFilePath());
    }

    void ensureIntermediateImageSets(LogicalSwapchain* pLogicalSwapchain, EffectGraph& graph, const size_t count)
    {
        graph.intermediateImageSets.reserve(count);
        graph.intermediateImageMemories.reserve(count);
        while (graph.intermediateImageSets.size() < count)
        {
            VkDeviceMemory memory = VK_NULL_HANDLE;
            auto imageInfo = pLogicalSwapchain->swapchainCreateInfo;
            imageInfo.imageFormat = graph.workingFormat;
            auto images = createFakeSwapchainImages(pLogicalSwapchain->pLogicalDevice,
                                                    imageInfo,
                                                    pLogicalSwapchain->imageCount,
                                                    memory);
            graph.intermediateImageSets.push_back(std::move(images));
            graph.intermediateImageMemories.push_back(memory);
            Logger::info("allocated a private image set for live effect chaining");
        }
    }

    void ensureNonMutableOutputImages(LogicalSwapchain* pLogicalSwapchain)
    {
        if (pLogicalSwapchain->pLogicalDevice->supportsMutableFormat || !pLogicalSwapchain->nonMutableOutputImages.empty())
            return;

        pLogicalSwapchain->nonMutableOutputImages = createFakeSwapchainImages(pLogicalSwapchain->pLogicalDevice,
                                                                              pLogicalSwapchain->swapchainCreateInfo,
                                                                              pLogicalSwapchain->imageCount,
                                                                              pLogicalSwapchain->nonMutableOutputMemory);
    }

    std::shared_ptr<Effect> createConfiguredEffect(LogicalSwapchain*          pLogicalSwapchain,
                                                   const std::string&         effectName,
                                                   const std::vector<VkImage>& inputImages,
                                                   const std::vector<VkImage>& outputImages,
                                                   Config* effectConfig,
                                                   VkFormat inputFormat,
                                                   VkFormat outputFormat,
                                                   VkFormat workingFormat = VK_FORMAT_UNDEFINED)
    {
        LogicalDevice* pLogicalDevice = pLogicalSwapchain->pLogicalDevice;
        if (workingFormat == VK_FORMAT_UNDEFINED) workingFormat = pLogicalSwapchain->effectFormat();
        const VkFormat unormFormat = convertToUNORM(workingFormat);
        const VkFormat srgbFormat = convertToSRGB(workingFormat);

        if (effectName == "fxaa")
            return std::make_shared<FxaaEffect>(
                pLogicalDevice, srgbFormat, pLogicalSwapchain->imageExtent, inputImages, outputImages, pConfig.get());
        if (effectName == "cas")
            return std::make_shared<CasEffect>(
                pLogicalDevice, convertToUNORM(outputFormat), pLogicalSwapchain->imageExtent,
                inputImages, outputImages, pConfig.get(), convertToUNORM(inputFormat), pLogicalSwapchain->hdr());
        if (effectName == "deband" || effectName == "makoDeband")
            return std::make_shared<DebandEffect>(
                pLogicalDevice, unormFormat, pLogicalSwapchain->imageExtent, inputImages, outputImages, pConfig.get());
        if (effectName == "smaa")
            return std::make_shared<SmaaEffect>(
                pLogicalDevice, unormFormat, pLogicalSwapchain->imageExtent, inputImages, outputImages, pConfig.get());
        if (effectName == "lut")
            return std::make_shared<LutEffect>(
                pLogicalDevice, unormFormat, pLogicalSwapchain->imageExtent, inputImages, outputImages, pConfig.get());
        if (effectName == "dls")
            return std::make_shared<DlsEffect>(
                pLogicalDevice, convertToUNORM(outputFormat), pLogicalSwapchain->imageExtent, inputImages, outputImages, pConfig.get(),
                pLogicalSwapchain->hdr(), convertToUNORM(inputFormat));
        auto module = compileReshadeModule(*effectConfig, effectName, pLogicalSwapchain->imageExtent,
                                           unormFormat == VK_FORMAT_A2R10G10B10_UNORM_PACK32);
        return std::make_shared<ReshadeEffect>(pLogicalDevice,
                                               workingFormat,
                                               pLogicalSwapchain->imageExtent,
                                               inputImages,
                                               outputImages,
                                               effectConfig,
                                               effectName, std::move(module), inputFormat);
    }

    VkFormat requestedEffectFormat(LogicalSwapchain* swapchain) {
        if (!swapchain->hdr() || !swapchain->hdrReducedPrecision)
            return swapchain->effectFormat();
        VkFormatProperties properties{};
        auto* device = swapchain->pLogicalDevice;
        device->vki.GetPhysicalDeviceFormatProperties(device->physicalDevice,
            VK_FORMAT_A2B10G10R10_UNORM_PACK32, &properties);
        VkFormatFeatureFlags required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
            VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
        if (swapchain->swapchainCreateInfo.imageUsage & VK_IMAGE_USAGE_STORAGE_BIT)
            required |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
        const bool supported = (properties.optimalTilingFeatures & required) == required;
        if (!supported) Logger::warn("compact HDR buffers unsupported; retaining RGBA16F");
        return swapchain->effectFormat(supported);
    }

    std::shared_ptr<EffectGraph> buildEffectGraph(LogicalSwapchain* pLogicalSwapchain,
                                                  const std::vector<std::string>& effectNames)
    {
        CheckedConstruction checked;
        const auto key = effectGraphKey(effectNames);
        auto graph = std::make_shared<EffectGraph>();
        graph->device = pLogicalSwapchain->pLogicalDevice;
        graph->workingFormat = requestedEffectFormat(pLogicalSwapchain);
        graph->config = std::make_shared<Config>(*pConfig);
        graph->effectNames = effectNames;
        LogicalDevice* pLogicalDevice = pLogicalSwapchain->pLogicalDevice;

        if (effectNames.empty())
        {
            graph->effects.push_back(std::make_shared<TransferEffect>(pLogicalDevice,
                                                                      pLogicalSwapchain->format,
                                                                      pLogicalSwapchain->imageExtent,
                                                                      pLogicalSwapchain->fakeImages,
                                                                      pLogicalSwapchain->images,
                                                                      pConfig.get()));
        }
        else
        {
            ensureIntermediateImageSets(pLogicalSwapchain, *graph, pLogicalSwapchain->intermediateCount(effectNames, graph->workingFormat));
            ensureNonMutableOutputImages(pLogicalSwapchain);
            const auto& finalImages = pLogicalDevice->supportsMutableFormat
                                          ? pLogicalSwapchain->images
                                          : pLogicalSwapchain->nonMutableOutputImages;

            const bool inputPass = pLogicalSwapchain->needsHdrInputPass(effectNames);
            const bool outputPass = pLogicalSwapchain->needsHdrOutputPass(effectNames, graph->workingFormat);
            const bool linear = pLogicalSwapchain->colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
            if (inputPass) {
                graph->effects.push_back(std::make_shared<HdrEffect>(pLogicalDevice,
                    pLogicalSwapchain->format, graph->workingFormat,
                    pLogicalSwapchain->imageExtent, pLogicalSwapchain->fakeImages,
                    graph->intermediateImageSets[0], graph->config.get(), linear ? 1 : 0));
            }
            const size_t stages = effectNames.size() + inputPass + outputPass;
            for (size_t i = 0; i < effectNames.size(); ++i) {
                const size_t stage = i + inputPass;
                const bool first = stage == 0;
                const bool last = stage + 1 == stages;
                const auto& inputImages = first ? pLogicalSwapchain->fakeImages
                    : graph->intermediateImageSets[stage - 1];
                const auto& outputImages = last ? finalImages
                    : graph->intermediateImageSets[stage];
                graph->effects.push_back(createConfiguredEffect(pLogicalSwapchain, effectNames[i],
                    inputImages, outputImages, graph->config.get(),
                    first ? pLogicalSwapchain->format : graph->workingFormat,
                    last ? pLogicalSwapchain->format : graph->workingFormat, graph->workingFormat));
            }
            if (outputPass) {
                graph->effects.push_back(std::make_shared<HdrEffect>(pLogicalDevice,
                    graph->workingFormat, pLogicalSwapchain->format,
                    pLogicalSwapchain->imageExtent, graph->intermediateImageSets[stages - 2],
                    finalImages, graph->config.get(), linear ? 2 : 0));
            }

            if (!pLogicalDevice->supportsMutableFormat)
            {
                graph->effects.push_back(std::make_shared<TransferEffect>(pLogicalDevice,
                                                                          pLogicalSwapchain->format,
                                                                          pLogicalSwapchain->imageExtent,
                                                                          pLogicalSwapchain->nonMutableOutputImages,
                                                                          pLogicalSwapchain->images,
                                                                          pConfig.get()));
            }
        }

        const auto depth = graph->usesDepthImage() ? pLogicalDevice->selectedDepthImage() : LogicalDevice::DepthImage{};
        const VkImageView depthImageView = depth.view;
        const VkImage depthImage = depth.image;
        const VkFormat depthFormat = depth.format;
        graph->depthImage = depthImage;
        ScopeExit rollback([&] { graph->destroy(pLogicalDevice); });
        graph->commandBuffers = allocateCommandBuffer(pLogicalDevice, pLogicalSwapchain->imageCount);
        writeCommandBuffers(pLogicalDevice, graph->effects, depthImage, depthImageView, depthFormat, graph->commandBuffers);
        graph->initializeDepthSubmissions(pLogicalDevice, pLogicalSwapchain->imageCount);
        Logger::info("prepared effect graph: " + (key.empty() ? std::string("off") : key) +
            "; working_format=" + std::to_string(graph->workingFormat));
        rollback.release();
        return graph;
    }

    void updateLiveEffectGraph(LogicalSwapchain* swapchain)
    {
        const uint64_t revision = pConfig->revision();
        if ((swapchain->effectSelectionRevision == revision &&
             swapchain->effectPrecisionRevision == swapchain->hdrPrecisionRevision) || !swapchain->activeEffectGraph)
            return;

        const auto requested = pConfig->getOption<std::vector<std::string>>("effects", {"cas"});
        const auto& active = swapchain->activeEffectGraph;
        const bool precisionChanged = requestedEffectFormat(swapchain) != active->workingFormat;
        swapchain->effectPrecisionRevision = swapchain->hdrPrecisionRevision;
        if (requested == active->effectNames && (requested.empty() ||
                (!precisionChanged && !pConfig->effectOptionsChanged(*active->config))))
        {
            swapchain->effectSelectionRevision = revision;
            return;
        }
        if (!canChangeEffectSelectionLive(requested))
        {
            swapchain->effectSelectionRevision = revision;
            Logger::warn("ignored invalid live effect selection");
            return;
        }

        auto* device = swapchain->pLogicalDevice;
        // The same queue-drain boundary is used for bundled and custom edits.
        // Keep the previous graph alive until a complete candidate is ready.
        const VkResult idleResult = device->vkd.QueueWaitIdle(device->queue);
        if (idleResult != VK_SUCCESS)
        {
            swapchain->effectSelectionRevision = revision;
            Logger::warn("could not retire the previous live effect graph because the graphics queue did not become idle");
            return;
        }

        device->retirePendingUploads();
        const auto key = effectGraphKey(requested);
        std::shared_ptr<EffectGraph> candidate;
        try
        {
            candidate = requested.empty() ? swapchain->effectGraphs.at("") : buildEffectGraph(swapchain, requested);
        }
        catch (const std::exception& error)
        {
            // Do not recompile the same bad edit on every present. A later
            // configuration revision can retry; the running graph is unchanged.
            swapchain->effectSelectionRevision = revision;
            Logger::warn(std::string("live effect update rejected; keeping previous graph: ") + error.what());
            return;
        }

        // Construction can submit texture uploads. Drain again before retiring
        // graph-owned resources; no per-frame wait is added to unchanged graphs.
        if (device->vkd.QueueWaitIdle(device->queue) != VK_SUCCESS)
        {
            // A device/queue failure cannot establish safe destruction. Leave
            // the candidate owned until swapchain teardown and retry later.
            swapchain->effectGraphs.emplace("pending/" + std::to_string(revision) + "/" +
                std::to_string(swapchain->hdrPrecisionRevision), candidate);
            swapchain->effectSelectionRevision = revision;
            Logger::warn("could not commit the prepared live effect graph");
            return;
        }
        swapchain->effectSelectionRevision = revision;
        swapchain->activeEffectGraph = candidate;
        size_t retiredGraphs = 0;
        for (auto graph = swapchain->effectGraphs.begin(); graph != swapchain->effectGraphs.end();)
        {
            if (graph->first.empty())
            {
                ++graph;
                continue;
            }
            graph->second->destroy(device);
            graph = swapchain->effectGraphs.erase(graph);
            ++retiredGraphs;
        }
        swapchain->effectGraphs[key] = candidate;
        Logger::info("activated live effect graph: " + (key.empty() ? std::string("off") : key));
        if (retiredGraphs > 0)
            Logger::info("retired " + std::to_string(retiredGraphs) + " inactive live effect graph(s)");
    }

    void rerecordEffectGraphs(LogicalSwapchain* swapchain,
                              const VkImage depthImage,
                              const VkImageView depthImageView,
                              const VkFormat depthFormat)
    {
        auto* device = swapchain->pLogicalDevice;
        for (auto& [key, graph] : swapchain->effectGraphs)
        {
            if (!graph->usesDepthImage() || graph->depthImage == depthImage)
                continue;
            if (device->depthUpdateResult != VK_SUCCESS)
                return;
            // Image destruction can come from a loading thread while the
            // application submits on its queues. Wait only for our fences;
            // QueueWaitIdle here would require the application's queue lock.
            device->depthUpdateResult = graph->waitForDepthSubmissions(device);
            if (device->depthUpdateResult != VK_SUCCESS) {
                Logger::err("depth graph retirement failed; result=" + std::to_string(device->depthUpdateResult));
                return;
            }
            try {
                CheckedConstruction checked;
                auto commands = allocateCommandBuffer(device, swapchain->imageCount);
                ScopeExit rollback([&] {
                    device->vkd.FreeCommandBuffers(device->device, device->commandPool,
                                                   commands.size(), commands.data());
                });
                writeCommandBuffers(device, graph->effects, depthImage, depthImageView, depthFormat, commands);
                if (!graph->commandBuffers.empty())
                    device->vkd.FreeCommandBuffers(device->device, device->commandPool,
                                                   graph->commandBuffers.size(), graph->commandBuffers.data());
                graph->commandBuffers = std::move(commands);
                graph->depthImage = depthImage;
                rollback.release();
            } catch (const std::exception& error) {
                // Descriptors may already reference the replacement. Never
                // resubmit the previous commands after a partial rewrite.
                device->depthUpdateResult = VK_ERROR_DEVICE_LOST;
                Logger::err(std::string("depth graph replacement failed: ") + error.what());
                return;
            }
        }
    }

    void updateDepthEffectGraphs(LogicalDevice* device)
    {
        const auto depth = device->selectedDepthImage();
        for (auto& [handle, swapchain] : swapchainMap)
            if (swapchain->pLogicalDevice == device)
                rerecordEffectGraphs(swapchain.get(), depth.image, depth.view, depth.format);
    }

    VkResult VKAPI_CALL vkBasalt_CreateInstance(const VkInstanceCreateInfo*  pCreateInfo,
                                                const VkAllocationCallbacks* pAllocator,
                                                VkInstance*                  pInstance)
    {
        VkLayerInstanceCreateInfo* layerCreateInfo = (VkLayerInstanceCreateInfo*) pCreateInfo->pNext;

        // step through the chain of pNext until we get to the link info
        while (layerCreateInfo
               && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO || layerCreateInfo->function != VK_LAYER_LINK_INFO))
        {
            layerCreateInfo = (VkLayerInstanceCreateInfo*) layerCreateInfo->pNext;
        }

        Logger::trace("vkCreateInstance");

        if (layerCreateInfo == nullptr)
        {
            // No loader instance create info
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        PFN_vkGetInstanceProcAddr gpa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
        // move chain on for next layer
        layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

        PFN_vkCreateInstance createFunc = (PFN_vkCreateInstance) gpa(VK_NULL_HANDLE, "vkCreateInstance");

        VkInstanceCreateInfo modifiedCreateInfo = *pCreateInfo;
        VkApplicationInfo    appInfo;
        if (modifiedCreateInfo.pApplicationInfo)
        {
            appInfo = *(modifiedCreateInfo.pApplicationInfo);
            if (appInfo.apiVersion < VK_API_VERSION_1_1)
            {
                appInfo.apiVersion = VK_API_VERSION_1_1;
            }
        }
        else
        {
            appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pNext              = nullptr;
            appInfo.pApplicationName   = nullptr;
            appInfo.applicationVersion = 0;
            appInfo.pEngineName        = nullptr;
            appInfo.engineVersion      = 0;
            appInfo.apiVersion         = VK_API_VERSION_1_1;
        }

        modifiedCreateInfo.pApplicationInfo = &appInfo;
        VkResult ret                        = createFunc(&modifiedCreateInfo, pAllocator, pInstance);

        // Failed creation has no valid dispatchable instance to inspect.
        if (ret != VK_SUCCESS)
            return ret;

        // fetch our own dispatch table for the functions we need, into the next layer
        InstanceDispatch dispatchTable;
        fillDispatchTableInstance(*pInstance, gpa, &dispatchTable);

        // store the table by key
        {
            scoped_lock l(globalLock);
            instanceDispatchMap[GetKey(*pInstance)] = dispatchTable;
            instanceMap[GetKey(*pInstance)]         = *pInstance;
            instanceVersionMap[GetKey(*pInstance)]  = modifiedCreateInfo.pApplicationInfo->apiVersion;
        }

        return ret;
    }

    void VKAPI_CALL vkBasalt_DestroyInstance(VkInstance instance, const VkAllocationCallbacks* pAllocator)
    {
        if (!instance)
            return;

        scoped_lock l(globalLock);

        Logger::trace("vkDestroyInstance");

        InstanceDispatch dispatchTable = instanceDispatchMap[GetKey(instance)];

        dispatchTable.DestroyInstance(instance, pAllocator);

        instanceDispatchMap.erase(GetKey(instance));
        instanceMap.erase(GetKey(instance));
        instanceVersionMap.erase(GetKey(instance));
    }

    VkResult VKAPI_CALL vkBasalt_CreateDevice(VkPhysicalDevice             physicalDevice,
                                              const VkDeviceCreateInfo*    pCreateInfo,
                                              const VkAllocationCallbacks* pAllocator,
                                              VkDevice*                    pDevice)
    {
        scoped_lock l(globalLock);
        Logger::trace("vkCreateDevice");
        VkLayerDeviceCreateInfo* layerCreateInfo = (VkLayerDeviceCreateInfo*) pCreateInfo->pNext;

        // step through the chain of pNext until we get to the link info
        while (layerCreateInfo
               && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO || layerCreateInfo->function != VK_LAYER_LINK_INFO))
        {
            layerCreateInfo = (VkLayerDeviceCreateInfo*) layerCreateInfo->pNext;
        }

        if (layerCreateInfo == nullptr)
        {
            // No loader instance create info
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        PFN_vkGetInstanceProcAddr gipa = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
        PFN_vkGetDeviceProcAddr   gdpa = layerCreateInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;
        // move chain on for next layer
        layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

        PFN_vkCreateDevice createFunc = (PFN_vkCreateDevice) gipa(VK_NULL_HANDLE, "vkCreateDevice");

        // check and activate extentions
        uint32_t extensionCount = 0;

        instanceDispatchMap[GetKey(physicalDevice)].EnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensionProperties(extensionCount);
        instanceDispatchMap[GetKey(physicalDevice)].EnumerateDeviceExtensionProperties(
            physicalDevice, nullptr, &extensionCount, extensionProperties.data());

        bool supportsMutableFormat = false;
        for (VkExtensionProperties properties : extensionProperties)
        {
            if (properties.extensionName == std::string("VK_KHR_swapchain_mutable_format"))
            {
                Logger::debug("device supports VK_KHR_swapchain_mutable_format");
                supportsMutableFormat = true;
                break;
            }
        }

        VkPhysicalDeviceProperties deviceProps;
        instanceDispatchMap[GetKey(physicalDevice)].GetPhysicalDeviceProperties(physicalDevice, &deviceProps);

        VkDeviceCreateInfo       modifiedCreateInfo = *pCreateInfo;
        std::vector<const char*> enabledExtensionNames;
        if (modifiedCreateInfo.enabledExtensionCount)
        {
            enabledExtensionNames = std::vector<const char*>(modifiedCreateInfo.ppEnabledExtensionNames,
                                                             modifiedCreateInfo.ppEnabledExtensionNames + modifiedCreateInfo.enabledExtensionCount);
        }

        if (supportsMutableFormat)
        {
            Logger::debug("activating mutable_format");
            addUniqueCString(enabledExtensionNames, "VK_KHR_swapchain_mutable_format");
        }
        if (deviceProps.apiVersion < VK_API_VERSION_1_2 || instanceVersionMap[GetKey(physicalDevice)] < VK_API_VERSION_1_2)
        {
            addUniqueCString(enabledExtensionNames, "VK_KHR_image_format_list");
        }
        modifiedCreateInfo.ppEnabledExtensionNames = enabledExtensionNames.data();
        modifiedCreateInfo.enabledExtensionCount   = enabledExtensionNames.size();

        // Active needed Features
        VkPhysicalDeviceFeatures deviceFeatures = {};
        if (modifiedCreateInfo.pEnabledFeatures)
        {
            deviceFeatures = *(modifiedCreateInfo.pEnabledFeatures);
        }
        deviceFeatures.shaderImageGatherExtended = VK_TRUE;
        modifiedCreateInfo.pEnabledFeatures      = &deviceFeatures;

        VkResult ret = createFunc(physicalDevice, &modifiedCreateInfo, pAllocator, pDevice);

        if (ret != VK_SUCCESS)
            return ret;

        std::shared_ptr<LogicalDevice> pLogicalDevice(new LogicalDevice());
        pLogicalDevice->vki                   = instanceDispatchMap[GetKey(physicalDevice)];
        pLogicalDevice->device                = *pDevice;
        pLogicalDevice->physicalDevice        = physicalDevice;
        pLogicalDevice->instance              = instanceMap[GetKey(physicalDevice)];
        pLogicalDevice->queue                 = VK_NULL_HANDLE;
        pLogicalDevice->queueFamilyIndex      = 0;
        pLogicalDevice->commandPool           = VK_NULL_HANDLE;
        pLogicalDevice->supportsMutableFormat = supportsMutableFormat;

        fillDispatchTableDevice(*pDevice, gdpa, &pLogicalDevice->vkd);

        uint32_t count;

        pLogicalDevice->vki.GetPhysicalDeviceQueueFamilyProperties(pLogicalDevice->physicalDevice, &count, nullptr);

        std::vector<VkQueueFamilyProperties> queueProperties(count);

        pLogicalDevice->vki.GetPhysicalDeviceQueueFamilyProperties(pLogicalDevice->physicalDevice, &count, queueProperties.data());
        for (uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; i++)
        {
            auto& queueInfo = pCreateInfo->pQueueCreateInfos[i];
            if ((queueProperties[queueInfo.queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                pLogicalDevice->vkd.GetDeviceQueue(pLogicalDevice->device, queueInfo.queueFamilyIndex, 0, &pLogicalDevice->queue);

                VkCommandPoolCreateInfo commandPoolCreateInfo;
                commandPoolCreateInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                commandPoolCreateInfo.pNext            = nullptr;
                commandPoolCreateInfo.flags            = 0;
                commandPoolCreateInfo.queueFamilyIndex = queueInfo.queueFamilyIndex;

                Logger::debug("Found graphics capable queue");
                pLogicalDevice->vkd.CreateCommandPool(pLogicalDevice->device, &commandPoolCreateInfo, nullptr, &pLogicalDevice->commandPool);
                pLogicalDevice->queueFamilyIndex = queueInfo.queueFamilyIndex;

                initializeDispatchTable(pLogicalDevice->queue, pLogicalDevice->device);

                break;
            }
        }

        if (!pLogicalDevice->queue)
            Logger::err("Did not find a graphics queue!");

        deviceMap[GetKey(*pDevice)] = pLogicalDevice;

        return VK_SUCCESS;
    }

    void VKAPI_CALL vkBasalt_DestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator)
    {
        if (!device)
            return;

        scoped_lock l(globalLock);

        Logger::trace("vkDestroyDevice");

        const auto deviceKey = GetKey(device);
        LogicalDevice* pLogicalDevice = deviceMap[deviceKey].get();
        if (!pLogicalDevice->pendingUploads.empty()) {
            const auto idle = pLogicalDevice->vkd.DeviceWaitIdle(device);
            if (idle == VK_SUCCESS || idle == VK_ERROR_DEVICE_LOST)
                pLogicalDevice->retirePendingUploads();
        }
        if (pLogicalDevice->commandPool != VK_NULL_HANDLE && pLogicalDevice->pendingUploads.empty())
            pLogicalDevice->vkd.DestroyCommandPool(device, pLogicalDevice->commandPool, pAllocator);
        pLogicalDevice->vkd.DestroyDevice(device, pAllocator);
        // If quiescence could not be established, driver teardown releases the
        // remaining device objects; CPU owners must not call a dead dispatch.
        pLogicalDevice->destroyed = true;
        pLogicalDevice->pendingUploads.clear();
        pLogicalDevice->pendingUploadEffects.clear();
        deviceMap.erase(deviceKey);
    }

    VKAPI_ATTR VkResult VKAPI_CALL vkBasalt_CreateSwapchainKHR(VkDevice                        device,
                                                               const VkSwapchainCreateInfoKHR* pCreateInfo,
                                                               const VkAllocationCallbacks*    pAllocator,
                                                               VkSwapchainKHR*                 pSwapchain)
    {
        scoped_lock l(globalLock);

        Logger::trace("vkCreateSwapchainKHR");

        LogicalDevice* pLogicalDevice = deviceMap[GetKey(device)].get();

        VkSwapchainCreateInfoKHR modifiedCreateInfo = *pCreateInfo;

        VkFormat format = modifiedCreateInfo.imageFormat;

        VkFormat srgbFormat  = isSRGB(format) ? format : convertToSRGB(format);
        VkFormat unormFormat = isSRGB(format) ? convertToUNORM(format) : format;
        Logger::debug(std::to_string(srgbFormat) + " " + std::to_string(unormFormat));

        VkFormat formats[] = {unormFormat, srgbFormat};

        VkImageFormatListCreateInfoKHR imageFormatListCreateInfo;
        if (pLogicalDevice->supportsMutableFormat)
        {
            modifiedCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                            | VK_IMAGE_USAGE_SAMPLED_BIT; // we want to use the swapchain images as output of the graphics pipeline
            modifiedCreateInfo.flags |= VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR;
            // TODO what if the application already uses multiple formats for the swapchain?

            imageFormatListCreateInfo.sType           = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR;
            imageFormatListCreateInfo.pNext           = modifiedCreateInfo.pNext;
            imageFormatListCreateInfo.viewFormatCount = (srgbFormat == unormFormat) ? 1 : 2;
            imageFormatListCreateInfo.pViewFormats    = formats;

            modifiedCreateInfo.pNext = &imageFormatListCreateInfo;
        }

        modifiedCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        Logger::debug("format " + std::to_string(modifiedCreateInfo.imageFormat));
        std::shared_ptr<LogicalSwapchain> pLogicalSwapchain(new LogicalSwapchain());
        pLogicalSwapchain->pLogicalDevice      = pLogicalDevice;
        pLogicalSwapchain->swapchainCreateInfo = *pCreateInfo;
        pLogicalSwapchain->imageExtent         = modifiedCreateInfo.imageExtent;
        pLogicalSwapchain->format              = modifiedCreateInfo.imageFormat;
        pLogicalSwapchain->colorSpace          = pCreateInfo->imageColorSpace;
        pLogicalSwapchain->imageCount          = 0;

        VkResult result = pLogicalDevice->vkd.CreateSwapchainKHR(device, &modifiedCreateInfo, pAllocator, pSwapchain);

        if (result == VK_SUCCESS)
            swapchainMap[*pSwapchain] = pLogicalSwapchain;

        return result;
    }

    // Private MAKO ABI v1: called after lower creation and before image exposure.
    // It supplies colour interpretation only; it does not alter WSI transport.
    VkBool32 VKAPI_CALL makoSetSwapchainColorSpaceV1(VkDevice device,
            VkSwapchainKHR swapchain, VkColorSpaceKHR colorSpace) {
        scoped_lock l(globalLock);
        const auto found = swapchainMap.find(swapchain);
        if (found == swapchainMap.end() || found->second->pLogicalDevice->device != device ||
                !found->second->fakeImages.empty())
            return VK_FALSE;
        const auto format = found->second->format;
        if ((colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT &&
                format != VK_FORMAT_A2B10G10R10_UNORM_PACK32 && format != VK_FORMAT_A2R10G10B10_UNORM_PACK32) ||
            (colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT && format != VK_FORMAT_R16G16B16A16_SFLOAT))
            return VK_FALSE;
        if (colorSpace != VK_COLOR_SPACE_HDR10_ST2084_EXT &&
            colorSpace != VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT && colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return VK_FALSE;
        found->second->colorSpace = colorSpace;
        return VK_TRUE;
    }

    // Private MAKO ABI v1. A live request is applied by the existing graph
    // transaction; immutable application formats and colour spaces stay intact.
    VkBool32 VKAPI_CALL makoSetSwapchainHdrPrecisionV1(VkDevice device,
            VkSwapchainKHR swapchain, VkBool32 reduced) {
        scoped_lock l(globalLock);
        const auto found = swapchainMap.find(swapchain);
        if (found == swapchainMap.end() || found->second->pLogicalDevice->device != device ||
                (reduced != VK_FALSE && reduced != VK_TRUE)) return VK_FALSE;
        auto& state = *found->second;
        if (state.hdrReducedPrecision != static_cast<bool>(reduced)) {
            state.hdrReducedPrecision = reduced;
            ++state.hdrPrecisionRevision;
        }
        return VK_TRUE;
    }

    VKAPI_ATTR VkResult VKAPI_CALL vkBasalt_GetSwapchainImagesKHR(VkDevice       device,
                                                                  VkSwapchainKHR swapchain,
                                                                  uint32_t*      pCount,
                                                                  VkImage*       pSwapchainImages)
    {
        scoped_lock l(globalLock);
        Logger::trace("vkGetSwapchainImagesKHR " + std::to_string(*pCount));

        LogicalDevice* pLogicalDevice = deviceMap[GetKey(device)].get();

        if (pSwapchainImages == nullptr)
        {
            return pLogicalDevice->vkd.GetSwapchainImagesKHR(device, swapchain, pCount, pSwapchainImages);
        }

        LogicalSwapchain* pLogicalSwapchain = swapchainMap[swapchain].get();

        // If the images got already requested once, return them again instead of creating new images
        if (pLogicalSwapchain->fakeImages.size())
        {
            *pCount = std::min<uint32_t>(*pCount, pLogicalSwapchain->imageCount);
            std::memcpy(pSwapchainImages, pLogicalSwapchain->fakeImages.data(), sizeof(VkImage) * (*pCount));
            return *pCount < pLogicalSwapchain->imageCount ? VK_INCOMPLETE : VK_SUCCESS;
        }

        try
        {
            CheckedConstruction checked;
            ASSERT_VULKAN(pLogicalDevice->vkd.GetSwapchainImagesKHR(device, swapchain, &pLogicalSwapchain->imageCount, nullptr));
            pLogicalSwapchain->images.resize(pLogicalSwapchain->imageCount);
            ASSERT_VULKAN(pLogicalDevice->vkd.GetSwapchainImagesKHR(device, swapchain, &pLogicalSwapchain->imageCount, pLogicalSwapchain->images.data()));

            pLogicalSwapchain->fakeImages = createFakeSwapchainImages(pLogicalDevice,
                                                                      pLogicalSwapchain->swapchainCreateInfo,
                                                                      pLogicalSwapchain->imageCount,
                                                                      pLogicalSwapchain->fakeImageMemory);
            Logger::debug("created application-facing fake swapchain images");

            const auto effectStrings = pConfig->getOption<std::vector<std::string>>("effects", {"cas"});
            auto fallback = buildEffectGraph(pLogicalSwapchain, {});
            pLogicalSwapchain->effectGraphs.emplace("", fallback);
            pLogicalSwapchain->activeEffectGraph = fallback;
            try
            {
                if (!effectStrings.empty())
                {
                    auto graph = buildEffectGraph(pLogicalSwapchain, effectStrings);
                    pLogicalSwapchain->effectGraphs.emplace(effectGraphKey(effectStrings), graph);
                    pLogicalSwapchain->activeEffectGraph = graph;
                }
            }
            catch (const std::exception& error)
            {
                Logger::warn(std::string("initial effect graph rejected; using passthrough: ") + error.what());
            }
            pLogicalSwapchain->effectSelectionRevision = pConfig->revision();
            pLogicalSwapchain->effectPrecisionRevision = pLogicalSwapchain->hdrPrecisionRevision;
            pLogicalSwapchain->semaphores = createSemaphores(pLogicalDevice, pLogicalSwapchain->imageCount);
            Logger::debug("created semaphores");
            Logger::trace("vkGetSwapchainImagesKHR");

            *pCount = std::min<uint32_t>(*pCount, pLogicalSwapchain->imageCount);
            std::memcpy(pSwapchainImages, pLogicalSwapchain->fakeImages.data(), sizeof(VkImage) * (*pCount));
            return *pCount < pLogicalSwapchain->imageCount ? VK_INCOMPLETE : VK_SUCCESS;
        }
        catch (const VulkanError& error)
        {
            pLogicalSwapchain->destroy();
            Logger::warn(std::string("swapchain shader resources unavailable: ") + error.what());
            return error.result;
        }
        catch (const std::exception& error)
        {
            pLogicalSwapchain->destroy();
            Logger::warn(std::string("swapchain shader preparation failed: ") + error.what());
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    VKAPI_ATTR VkResult VKAPI_CALL vkBasalt_QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
    {
        scoped_lock l(globalLock);

        reloadConfigIfChanged();

        static uint32_t keySymbol = convertToKeySym(pConfig->getOption<std::string>("toggleKey", "Home"));

        static bool          pressed       = false;
        static bool          presentEffect = pConfig->getOption<bool>("enableOnLaunch", true);
        static ToggleKeyPoll keyPoll;

        if (keyPoll.due(std::chrono::steady_clock::now()))
        {
            if (isKeyPressed(keySymbol))
            {
                if (!pressed)
                {
                    presentEffect = !presentEffect;
                    pressed       = true;
                }
            }
            else
            {
                pressed = false;
            }
        }

        LogicalDevice* pLogicalDevice = deviceMap[GetKey(queue)].get();

        if (pLogicalDevice->depthUpdateResult != VK_SUCCESS)
            return pLogicalDevice->depthUpdateResult;

        std::vector<VkSemaphore> presentSemaphores;
        presentSemaphores.reserve(pPresentInfo->swapchainCount);

        std::vector<VkPipelineStageFlags> waitStages(pPresentInfo->waitSemaphoreCount, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        for (unsigned int i = 0; i < (*pPresentInfo).swapchainCount; i++)
        {
            uint32_t          index             = (*pPresentInfo).pImageIndices[i];
            VkSwapchainKHR    swapchain         = (*pPresentInfo).pSwapchains[i];
            LogicalSwapchain* pLogicalSwapchain = swapchainMap[swapchain].get();

            updateLiveEffectGraph(pLogicalSwapchain);
            for (auto& effect : pLogicalSwapchain->activeEffectGraph->effects)
            {
                effect->updateEffect(index);
            }

            VkSubmitInfo submitInfo;
            submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.pNext              = nullptr;
            submitInfo.waitSemaphoreCount = i == 0 ? pPresentInfo->waitSemaphoreCount : 0;
            submitInfo.pWaitSemaphores    = i == 0 ? pPresentInfo->pWaitSemaphores : nullptr;
            submitInfo.pWaitDstStageMask  = i == 0 ? waitStages.data() : nullptr;
            submitInfo.commandBufferCount = 1;
            const auto& selectedGraph = presentEffect
                                            ? pLogicalSwapchain->activeEffectGraph
                                            : pLogicalSwapchain->effectGraphs.at("");
            submitInfo.pCommandBuffers = &(selectedGraph->commandBuffers[index]);
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores    = &(pLogicalSwapchain->semaphores[index]);

            presentSemaphores.push_back(pLogicalSwapchain->semaphores[index]);

            VkFence completion = VK_NULL_HANDLE;
            VkResult vr = selectedGraph->prepareDepthSubmission(pLogicalDevice, index, completion);
            if (vr == VK_SUCCESS) {
                vr = pLogicalDevice->vkd.QueueSubmit(pLogicalDevice->queue, 1, &submitInfo, completion);
                if (vr == VK_SUCCESS) selectedGraph->depthSubmitted(index);
            }

            if (vr != VK_SUCCESS)
            {
                return vr;
            }
        }

        VkPresentInfoKHR presentInfo   = *pPresentInfo;
        presentInfo.waitSemaphoreCount = presentSemaphores.size();
        presentInfo.pWaitSemaphores    = presentSemaphores.data();

        return pLogicalDevice->vkd.QueuePresentKHR(queue, &presentInfo);
    }

    VKAPI_ATTR void VKAPI_CALL vkBasalt_DestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator)
    {
        if (!swapchain)
            return;

        scoped_lock l(globalLock);
        // we need to delete the infos of the oldswapchain

        Logger::trace("vkDestroySwapchainKHR " + convertToString(swapchain));
        swapchainMap[swapchain]->destroy();
        swapchainMap.erase(swapchain);
        LogicalDevice* pLogicalDevice = deviceMap[GetKey(device)].get();

        pLogicalDevice->vkd.DestroySwapchainKHR(device, swapchain, pAllocator);
    }

    VKAPI_ATTR VkResult VKAPI_CALL vkBasalt_CreateImage(VkDevice                     device,
                                                        const VkImageCreateInfo*     pCreateInfo,
                                                        const VkAllocationCallbacks* pAllocator,
                                                        VkImage*                     pImage)
    {
        scoped_lock l(globalLock);

        LogicalDevice* pLogicalDevice = deviceMap[GetKey(device)].get();
        if (isDepthFormat(pCreateInfo->format) && pCreateInfo->samples == VK_SAMPLE_COUNT_1_BIT
            && ((pCreateInfo->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
        {
            Logger::debug("detected depth image with format: " + convertToString(pCreateInfo->format));
            Logger::debug(std::to_string(pCreateInfo->extent.width) + "x" + std::to_string(pCreateInfo->extent.height));
            Logger::debug(
                std::to_string((pCreateInfo->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT));

            VkImageCreateInfo modifiedCreateInfo = *pCreateInfo;
            modifiedCreateInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
            VkResult result = pLogicalDevice->vkd.CreateImage(device, &modifiedCreateInfo, pAllocator, pImage);
            if (result == VK_SUCCESS)
                pLogicalDevice->depthImages.push_back({*pImage, pCreateInfo->format, VK_NULL_HANDLE});

            return result;
        }
        else
        {
            return pLogicalDevice->vkd.CreateImage(device, pCreateInfo, pAllocator, pImage);
        }
    }

    VKAPI_ATTR VkResult VKAPI_CALL vkBasalt_BindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset)
    {
        scoped_lock l(globalLock);

        LogicalDevice* pLogicalDevice = deviceMap[GetKey(device)].get();

        VkResult result = pLogicalDevice->vkd.BindImageMemory(device, image, memory, memoryOffset);
        if (result != VK_SUCCESS)
            return result;
        const auto depth = std::find_if(pLogicalDevice->depthImages.begin(), pLogicalDevice->depthImages.end(),
            [image](const auto& entry) { return entry.image == image; });
        if (depth != pLogicalDevice->depthImages.end() && depth->view == VK_NULL_HANDLE) {
            try {
                CheckedConstruction checked;
                depth->view = createImageViews(pLogicalDevice, depth->format, {image},
                                               VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT)[0];
                updateDepthEffectGraphs(pLogicalDevice);
            } catch (const std::exception& error) {
                // Optional depth capture must not change a successful bind.
                Logger::warn(std::string("depth view unavailable: ") + error.what());
            }
        }
        return result;
    }

    VKAPI_ATTR void VKAPI_CALL vkBasalt_DestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator)
    {
        if (!image)
            return;

        scoped_lock l(globalLock);

        LogicalDevice* pLogicalDevice = deviceMap[GetKey(device)].get();

        const auto depth = std::find_if(pLogicalDevice->depthImages.begin(), pLogicalDevice->depthImages.end(),
            [image](const auto& entry) { return entry.image == image; });
        if (depth != pLogicalDevice->depthImages.end()) {
            const auto view = depth->view;
            pLogicalDevice->depthImages.erase(depth);
            updateDepthEffectGraphs(pLogicalDevice);
            // Keep the view until any shader references have been drained and
            // removed. Unbound or modern BindImageMemory2 images have no view.
            if (view != VK_NULL_HANDLE && (pLogicalDevice->depthUpdateResult == VK_SUCCESS ||
                                          pLogicalDevice->depthUpdateResult == VK_ERROR_DEVICE_LOST))
                pLogicalDevice->vkd.DestroyImageView(pLogicalDevice->device, view, nullptr);
        }

        pLogicalDevice->vkd.DestroyImage(pLogicalDevice->device, image, pAllocator);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Enumeration function

    VkResult VKAPI_CALL vkBasalt_EnumerateInstanceLayerProperties(uint32_t* pPropertyCount, VkLayerProperties* pProperties)
    {
        if (pPropertyCount)
            *pPropertyCount = 1;

        if (pProperties)
        {
            std::strcpy(pProperties->layerName, VKBASALT_NAME);
            std::strcpy(pProperties->description, "a post processing layer");
            pProperties->implementationVersion = 1;
            pProperties->specVersion           = VKBASALT_API_VERSION;
        }

        return VK_SUCCESS;
    }

    VkResult VKAPI_CALL vkBasalt_EnumerateDeviceLayerProperties(VkPhysicalDevice   physicalDevice,
                                                                uint32_t*          pPropertyCount,
                                                                VkLayerProperties* pProperties)
    {
        return vkBasalt_EnumerateInstanceLayerProperties(pPropertyCount, pProperties);
    }

    VkResult VKAPI_CALL vkBasalt_EnumerateInstanceExtensionProperties(const char*            pLayerName,
                                                                      uint32_t*              pPropertyCount,
                                                                      VkExtensionProperties* pProperties)
    {
        if (pLayerName == NULL || std::strcmp(pLayerName, VKBASALT_NAME))
        {
            return VK_ERROR_LAYER_NOT_PRESENT;
        }

        // don't expose any extensions
        if (pPropertyCount)
        {
            *pPropertyCount = 0;
        }
        return VK_SUCCESS;
    }

    VkResult VKAPI_CALL vkBasalt_EnumerateDeviceExtensionProperties(VkPhysicalDevice       physicalDevice,
                                                                    const char*            pLayerName,
                                                                    uint32_t*              pPropertyCount,
                                                                    VkExtensionProperties* pProperties)
    {
        // pass through any queries that aren't to us
        if (pLayerName == NULL || std::strcmp(pLayerName, VKBASALT_NAME))
        {
            if (physicalDevice == VK_NULL_HANDLE)
            {
                return VK_SUCCESS;
            }

            scoped_lock l(globalLock);
            return instanceDispatchMap[GetKey(physicalDevice)].EnumerateDeviceExtensionProperties(
                physicalDevice, pLayerName, pPropertyCount, pProperties);
        }

        // don't expose any extensions
        if (pPropertyCount)
        {
            *pPropertyCount = 0;
        }
        return VK_SUCCESS;
    }
} // namespace vkBasalt

extern "C"
{ // these are the entry points for the layer, so they need to be c-linkeable

    VK_BASALT_EXPORT PFN_vkVoidFunction VKAPI_CALL vkBasalt_GetDeviceProcAddr(VkDevice device, const char* pName);
    VK_BASALT_EXPORT PFN_vkVoidFunction VKAPI_CALL vkBasalt_GetInstanceProcAddr(VkInstance instance, const char* pName);

#define GETPROCADDR(func) \
    if (!std::strcmp(pName, "vk" #func)) \
        return (PFN_vkVoidFunction) &vkBasalt::vkBasalt_##func;
    /*
    Return our funktions for the funktions we want to intercept
    the macro takes the name and returns our vkBasalt_##func, if the name is equal
    */

    // vkGetDeviceProcAddr needs to behave like vkGetInstanceProcAddr thanks to some games
#define INTERCEPT_CALLS \
    /* instance chain functions we intercept */ \
    if (!std::strcmp(pName, "vkGetInstanceProcAddr")) \
        return (PFN_vkVoidFunction) &vkBasalt_GetInstanceProcAddr; \
    GETPROCADDR(EnumerateInstanceLayerProperties); \
    GETPROCADDR(EnumerateInstanceExtensionProperties); \
    GETPROCADDR(CreateInstance); \
    GETPROCADDR(DestroyInstance); \
\
    /* device chain functions we intercept*/ \
    if (!std::strcmp(pName, "vkGetDeviceProcAddr")) \
        return (PFN_vkVoidFunction) &vkBasalt_GetDeviceProcAddr; \
    GETPROCADDR(EnumerateDeviceLayerProperties); \
    GETPROCADDR(EnumerateDeviceExtensionProperties); \
    GETPROCADDR(CreateDevice); \
    GETPROCADDR(DestroyDevice); \
    GETPROCADDR(CreateSwapchainKHR); \
    GETPROCADDR(GetSwapchainImagesKHR); \
    GETPROCADDR(QueuePresentKHR); \
    GETPROCADDR(DestroySwapchainKHR); \
\
    if (vkBasalt::pConfig->getOption<std::string>("depthCapture", "off") == "on") \
    { \
        GETPROCADDR(CreateImage); \
        GETPROCADDR(DestroyImage); \
        GETPROCADDR(BindImageMemory); \
    }

    VK_BASALT_EXPORT PFN_vkVoidFunction VKAPI_CALL vkBasalt_GetDeviceProcAddr(VkDevice device, const char* pName)
    {
        if (vkBasalt::pConfig == nullptr)
        {
            vkBasalt::pConfig = std::shared_ptr<vkBasalt::Config>(new vkBasalt::Config());
        }

        if (!std::strcmp(pName, "makoSetSwapchainHdrPrecisionV1"))
            return reinterpret_cast<PFN_vkVoidFunction>(&vkBasalt::makoSetSwapchainHdrPrecisionV1);
        if (!std::strcmp(pName, "makoSetSwapchainColorSpaceV1"))
            return reinterpret_cast<PFN_vkVoidFunction>(&vkBasalt::makoSetSwapchainColorSpaceV1);
        INTERCEPT_CALLS

        {
            vkBasalt::scoped_lock l(vkBasalt::globalLock);
            return vkBasalt::deviceMap[vkBasalt::GetKey(device)]->vkd.GetDeviceProcAddr(device, pName);
        }
    }

    VK_BASALT_EXPORT PFN_vkVoidFunction VKAPI_CALL vkBasalt_GetInstanceProcAddr(VkInstance instance, const char* pName)
    {
        if (vkBasalt::pConfig == nullptr)
        {
            vkBasalt::pConfig = std::shared_ptr<vkBasalt::Config>(new vkBasalt::Config());
        }

        if (!std::strcmp(pName, "makoSetSwapchainHdrPrecisionV1"))
            return reinterpret_cast<PFN_vkVoidFunction>(&vkBasalt::makoSetSwapchainHdrPrecisionV1);
        if (!std::strcmp(pName, "makoSetSwapchainColorSpaceV1"))
            return reinterpret_cast<PFN_vkVoidFunction>(&vkBasalt::makoSetSwapchainColorSpaceV1);
        INTERCEPT_CALLS

        {
            vkBasalt::scoped_lock l(vkBasalt::globalLock);
            return vkBasalt::instanceDispatchMap[vkBasalt::GetKey(instance)].GetInstanceProcAddr(instance, pName);
        }
    }

} // extern "C"
