#include "logical_swapchain.hpp"
#include "command_buffer.hpp"
#include <cassert>
#include <iostream>
#include <type_traits>

namespace vkBasalt {
extern std::unordered_map<void*, std::shared_ptr<LogicalDevice>> deviceMap;
extern std::unordered_map<VkSwapchainKHR, std::shared_ptr<LogicalSwapchain>> swapchainMap;
VkResult vkBasalt_CreateImage(VkDevice, const VkImageCreateInfo*, const VkAllocationCallbacks*, VkImage*);
VkResult vkBasalt_BindImageMemory(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize);
void vkBasalt_DestroyImage(VkDevice, VkImage, const VkAllocationCallbacks*);
void rerecordEffectGraphs(LogicalSwapchain*, VkImage, VkImageView, VkFormat);
}
namespace {
template<class T> T handle(uintptr_t n) {
    if constexpr (std::is_pointer_v<T>) return reinterpret_cast<T>(n);
    else return static_cast<T>(n);
}
uintptr_t dispatch=1, commandDispatch=1;
size_t nextImage=100, frees=0, barriers=0, drains=0, destroys=0;
bool pending=false, depthWork=false;
size_t fencesLive=0, fenceCreates=0, failFenceAt=0;
VkResult createResult=VK_SUCCESS, bindResult=VK_SUCCESS, drainResult=VK_SUCCESS;
struct TestEffect : vkBasalt::Effect {
    bool depth=false;
    VkImageView bound=VK_NULL_HANDLE;
    bool usesDepthImage() const override { return depth; }
    void applyEffect(uint32_t, VkCommandBuffer) override {}
    void useDepthImage(VkImageView view) override { assert(!pending); bound=view; }
};
}
int main(int argc, char** argv) {
    using namespace vkBasalt;
    auto d=std::make_shared<LogicalDevice>();
    d->device=reinterpret_cast<VkDevice>(&dispatch);
    d->queue=reinterpret_cast<VkQueue>(&dispatch);
    d->vkd.CreateImage=+[](VkDevice,const VkImageCreateInfo*,const VkAllocationCallbacks*,VkImage* out) {
        *out=createResult==VK_SUCCESS?handle<VkImage>(nextImage++):VK_NULL_HANDLE; return createResult; };
    d->vkd.BindImageMemory=+[](VkDevice,VkImage,VkDeviceMemory,VkDeviceSize) { return bindResult; };
    d->vkd.CreateImageView=+[](VkDevice,const VkImageViewCreateInfo* info,const VkAllocationCallbacks*,VkImageView* out) {
        *out=handle<VkImageView>(nextImage++); return VK_SUCCESS; };
    d->vkd.DestroyImageView=+[](VkDevice,VkImageView,const VkAllocationCallbacks*) { assert(!pending || !depthWork); ++destroys; };
    d->vkd.DestroyImage=+[](VkDevice,VkImage,const VkAllocationCallbacks*) {};
    d->vkd.QueueWaitIdle=+[](VkQueue) -> VkResult { assert(false && "depth lifetime must not access the application queue"); return VK_ERROR_UNKNOWN; };
    d->vkd.CreateFence=+[](VkDevice,const VkFenceCreateInfo*,const VkAllocationCallbacks*,VkFence* fence) {
        *fence=VK_NULL_HANDLE; if(++fenceCreates==failFenceAt) return VK_ERROR_OUT_OF_HOST_MEMORY;
        *fence=handle<VkFence>(nextImage++); ++fencesLive; return VK_SUCCESS; };
    d->vkd.DestroyFence=+[](VkDevice,VkFence,const VkAllocationCallbacks*) { assert(fencesLive>0); --fencesLive; };
    d->vkd.ResetFences=+[](VkDevice,uint32_t,const VkFence*) { assert(!pending); return VK_SUCCESS; };
    d->vkd.WaitForFences=+[](VkDevice,uint32_t,const VkFence*,VkBool32,uint64_t) {
        ++drains; if(drainResult==VK_SUCCESS) pending=false; return drainResult; };
    d->vkd.AllocateCommandBuffers=+[](VkDevice,const VkCommandBufferAllocateInfo* info,VkCommandBuffer* out) {
        assert(info->commandBufferCount==1); *out=reinterpret_cast<VkCommandBuffer>(&commandDispatch); return VK_SUCCESS; };
    d->vkd.FreeCommandBuffers=+[](VkDevice,VkCommandPool,uint32_t,const VkCommandBuffer*) {
        assert(!pending && "freed a pending shader command buffer during depth replacement"); ++frees; };
    d->vkd.BeginCommandBuffer=+[](VkCommandBuffer,const VkCommandBufferBeginInfo*) { return VK_SUCCESS; };
    d->vkd.EndCommandBuffer=+[](VkCommandBuffer) { return VK_SUCCESS; };
    d->vkd.CmdPipelineBarrier=+[](VkCommandBuffer,VkPipelineStageFlags,VkPipelineStageFlags,VkDependencyFlags,
        uint32_t,const VkMemoryBarrier*,uint32_t,const VkBufferMemoryBarrier*,uint32_t n,const VkImageMemoryBarrier*) { barriers+=n; };
    deviceMap[reinterpret_cast<void*>(dispatch)]=d;
    auto effect=std::make_shared<TestEffect>();
    auto g=std::make_shared<EffectGraph>(); g->effectNames={"test"}; g->effects={effect};
    g->commandBuffers={reinterpret_cast<VkCommandBuffer>(&commandDispatch)};
    auto s=std::make_shared<LogicalSwapchain>(); s->pLogicalDevice=d.get(); s->imageCount=1;
    s->effectGraphs["test"]=g; s->activeEffectGraph=g;
    swapchainMap[handle<VkSwapchainKHR>(1)]=s;
    auto imageInfo=VkImageCreateInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.format=VK_FORMAT_D32_SFLOAT; imageInfo.samples=VK_SAMPLE_COUNT_1_BIT;
    imageInfo.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    auto create=[&] { VkImage image{}; assert(vkBasalt_CreateImage(d->device,&imageInfo,nullptr,&image)==VK_SUCCESS); return image; };
    auto bind=[&](VkImage image) { assert(vkBasalt_BindImageMemory(d->device,image,handle<VkDeviceMemory>(7),0)==VK_SUCCESS); };
    const std::string mode=argc>1?argv[1]:"colour";
    if(mode=="colour") {
        auto a=create(); bind(a);
        const auto selected=d->selectedDepthImage();
        writeCommandBuffers(d.get(), g->effects, selected.image, selected.view, selected.format, g->commandBuffers);
        assert(frees==0 && barriers==0 && drains==0 && "colour-only graph unnecessarily rebuilt for depth");
        pending=true;
        vkBasalt_DestroyImage(d->device,a,nullptr);
        assert(frees==0 && barriers==0 && drains==0);
        pending=false;
    } else if(mode=="pending") {
        effect->depth=true; depthWork=true; g->initializeDepthSubmissions(d.get(),1);
        auto a=create(); bind(a); auto b=create(); bind(b);
        VkFence submitted{};
        assert(g->prepareDepthSubmission(d.get(),0,submitted)==VK_SUCCESS && submitted!=VK_NULL_HANDLE);
        g->depthSubmitted(0);
        const auto before=drains; pending=true;
        vkBasalt_DestroyImage(d->device,a,nullptr);
        assert(!pending && drains==before+1);
        assert(effect->bound!=VK_NULL_HANDLE);
        vkBasalt_DestroyImage(d->device,b,nullptr);
        assert(effect->bound==VK_NULL_HANDLE);
    } else if(mode=="unbound") {
        auto a=create(); vkBasalt_DestroyImage(d->device,a,nullptr);
        assert(destroys==0 && frees==0);
    } else if(mode=="fence-reuse") {
        effect->depth=true; depthWork=true; g->initializeDepthSubmissions(d.get(),1);
        VkFence first{}, second{};
        assert(g->prepareDepthSubmission(d.get(),0,first)==VK_SUCCESS);
        assert(first!=VK_NULL_HANDLE && drains==0);
        g->depthSubmitted(0); pending=true;
        assert(g->prepareDepthSubmission(d.get(),0,second)==VK_SUCCESS);
        assert(first==second && drains==1 && !pending);
        g->depthSubmitted(0); pending=true;
        assert(g->waitForDepthSubmissions(d.get())==VK_SUCCESS && drains==2 && !pending);
        assert(g->waitForDepthSubmissions(d.get())==VK_SUCCESS && drains==2);
    } else if(mode=="fence-rollback") {
        effect->depth=true; failFenceAt=2;
        bool failed=false;
        try { CheckedConstruction checked; g->initializeDepthSubmissions(d.get(),3); }
        catch(const VulkanError&) { failed=true; }
        assert(failed && fencesLive==1);
    } else if(mode=="failures") {
        createResult=VK_ERROR_OUT_OF_DEVICE_MEMORY;
        VkImage failed{};
        assert(vkBasalt_CreateImage(d->device,&imageInfo,nullptr,&failed)==createResult);
        assert(d->depthImages.empty());
        createResult=VK_SUCCESS;
        auto a=create(); bindResult=VK_ERROR_OUT_OF_DEVICE_MEMORY;
        assert(vkBasalt_BindImageMemory(d->device,a,handle<VkDeviceMemory>(7),0)==bindResult);
        assert(d->selectedDepthImage().view==VK_NULL_HANDLE);
        vkBasalt_DestroyImage(d->device,a,nullptr);
        assert(destroys==0 && frees==0 && drains==0);
    } else if(mode=="drain-failure") {
        effect->depth=true; depthWork=true; g->initializeDepthSubmissions(d.get(),1);
        auto a=create(); bind(a);
        const auto oldView=effect->bound; const auto oldFrees=frees;
        g->depthSubmitted(0); pending=true; drainResult=VK_ERROR_OUT_OF_HOST_MEMORY;
        vkBasalt_DestroyImage(d->device,a,nullptr);
        assert(d->depthUpdateResult==drainResult && effect->bound==oldView);
        assert(frees==oldFrees && destroys==0);
        pending=false;
    } else if(mode=="churn") {
        for(size_t i=0;i<256;++i) {
            auto a=create(); auto b=create(); bind(b); bind(a);
            pending=true;
            vkBasalt_DestroyImage(d->device,a,nullptr);
            vkBasalt_DestroyImage(d->device,b,nullptr);
            pending=false;
            assert(d->depthImages.empty() && frees==0 && drains==0 && barriers==0);
        }
    } else if(mode=="out-of-order") {
        effect->depth=true; depthWork=true; g->initializeDepthSubmissions(d.get(),1);
        auto a=create(); auto b=create(); bind(a);
        assert(effect->bound!=VK_NULL_HANDLE);
        bind(b); auto before=effect->bound;
        vkBasalt_DestroyImage(d->device,b,nullptr);
        assert(effect->bound==before);
        vkBasalt_DestroyImage(d->device,a,nullptr);
        assert(effect->bound==VK_NULL_HANDLE);
    }
    g->commandBuffers.clear(); g->destroy(d.get()); swapchainMap.clear(); deviceMap.clear();
    assert(fencesLive==0);
    std::cout<<"depth lifetime "<<mode<<" passed\n";
}
