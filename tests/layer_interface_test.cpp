#include "vulkan_include.hpp"
#include "vulkan_version.hpp"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <iostream>

extern "C" PFN_vkVoidFunction VKAPI_CALL vkBasalt_GetInstanceProcAddr(VkInstance, const char*);
namespace {
VkResult creationResult = VK_SUCCESS;
uint32_t requestedVersion = 0;
size_t dispatchQueries = 0, destroyed = 0;
void* dispatchKey = &dispatchKey;
VkInstance mockInstance = reinterpret_cast<VkInstance>(&dispatchKey);
const void* expectedNext = nullptr;
void VKAPI_CALL sentinel() {}
VkResult VKAPI_CALL create(const VkInstanceCreateInfo* info, const VkAllocationCallbacks*, VkInstance* output) {
    assert(info->pApplicationInfo->apiVersion == requestedVersion);
    assert(info->pNext == expectedNext);
    *output = creationResult == VK_SUCCESS ? mockInstance : VK_NULL_HANDLE;
    return creationResult;
}
void VKAPI_CALL destroy(VkInstance instance, const VkAllocationCallbacks*) {
    assert(instance == mockInstance);
    ++destroyed;
}
PFN_vkVoidFunction VKAPI_CALL next(VkInstance instance, const char* name) {
    if (!std::strcmp(name, "vkCreateInstance")) return reinterpret_cast<PFN_vkVoidFunction>(create);
    assert(instance == mockInstance);
    ++dispatchQueries;
    if (!std::strcmp(name, "vkDestroyInstance")) return reinterpret_cast<PFN_vkVoidFunction>(destroy);
    return sentinel;
}
}
int main(int argc, char** argv) {
    // Exercise the actual layer entry points and downstream call chain.
    auto createInstance = reinterpret_cast<PFN_vkCreateInstance>(vkBasalt_GetInstanceProcAddr(nullptr, "vkCreateInstance"));
    for (auto result : {VK_SUCCESS, VK_ERROR_INCOMPATIBLE_DRIVER, VK_ERROR_EXTENSION_NOT_PRESENT, VK_ERROR_OUT_OF_HOST_MEMORY}) {
        VkLayerInstanceLink link{};
        link.pfnNextGetInstanceProcAddr = next;
        VkLayerInstanceCreateInfo chain{};
        chain.sType = VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO;
        chain.function = VK_LAYER_LINK_INFO;
        chain.u.pLayerInfo = &link;
        expectedNext = &chain;
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.apiVersion = VK_API_VERSION_1_4;
        requestedVersion = app.apiVersion;
        VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        info.pNext = &chain;
        info.pApplicationInfo = &app;
        VkInstance instance = VK_NULL_HANDLE;
        creationResult = result;
        dispatchQueries = 0;
        assert(createInstance(&info, nullptr, &instance) == result);
        assert(app.apiVersion == VK_API_VERSION_1_4);
        if (result != VK_SUCCESS) {
            assert(instance == VK_NULL_HANDLE);
            assert(dispatchQueries == 0);
            continue;
        }
        assert(instance == mockInstance && dispatchQueries > 0);
        // New core commands that this layer does not intercept must pass through.
        assert(vkBasalt_GetInstanceProcAddr(instance, "vkMapMemory2") == sentinel);
        assert(vkBasalt_GetInstanceProcAddr(instance, "vkGetDeviceImageSubresourceLayout") == sentinel);
        auto destroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(vkBasalt_GetInstanceProcAddr(instance, "vkDestroyInstance"));
        destroyInstance(instance, nullptr);
    }
    assert(destroyed == 1);
    auto enumerate = reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(vkBasalt_GetInstanceProcAddr(nullptr, "vkEnumerateInstanceLayerProperties"));
    uint32_t count = 1;
    VkLayerProperties properties{};
    assert(enumerate(&count, &properties) == VK_SUCCESS && count == 1);
    assert(properties.specVersion == VKBASALT_API_VERSION);
    const std::string version = std::to_string(VK_API_VERSION_MAJOR(properties.specVersion)) + "." +
        std::to_string(VK_API_VERSION_MINOR(properties.specVersion)) + "." + std::to_string(VK_API_VERSION_PATCH(properties.specVersion));
    assert(argc >= 3);
    for (int index = 1; index < argc; ++index) {
        std::ifstream file(argv[index]);
        assert(file.is_open());
        const std::string manifest((std::istreambuf_iterator<char>(file)), {});
        assert(manifest.find("\"api_version\": \"" + version + "\"") != std::string::npos);
    }
    std::cout << "Vulkan " << version << " layer declarations, forwarding, and creation failures passed\n";
}
