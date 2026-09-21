#ifndef EFFECT_CAS_HPP_INCLUDED
#define EFFECT_CAS_HPP_INCLUDED
#include <vector>
#include <fstream>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>

#include "vulkan_include.hpp"

#include "effect_simple.hpp"
#include "config.hpp"

namespace vkBasalt
{
    class CasEffect : public SimpleEffect
    {
    public:
        CasEffect(LogicalDevice*       pLogicalDevice,
                  VkFormat             format,
                  VkExtent2D           imageExtent,
                  std::vector<VkImage> inputImages,
                  std::vector<VkImage> outputImages,
                  Config*              pConfig);
        void updateEffect(uint32_t imageIndex) override;
        ~CasEffect();

    private:
        std::vector<uint64_t> appliedConfigRevisions;
    };
} // namespace vkBasalt

#endif // EFFECT_CAS_HPP_INCLUDED
