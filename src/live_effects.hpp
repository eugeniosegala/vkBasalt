#ifndef LIVE_EFFECTS_HPP_INCLUDED
#define LIVE_EFFECTS_HPP_INCLUDED

#include <string>
#include <vector>

namespace vkBasalt
{
    inline bool isMakoControlledEffect(const std::string& effect)
    {
        return effect == "fxaa" || effect == "smaa" || effect == "cas" || effect == "dls"
               || effect == "makoVibrance" || effect == "makoCurves" || effect == "makoDeband"
               || effect == "makoTechnicolor" || effect == "makoSepia"
               || effect == "makoMonochrome" || effect == "makoVignette";
    }

    inline std::vector<std::string> makoControlledEffects(const std::vector<std::string>& effects)
    {
        std::vector<std::string> controlled;
        for (const auto& effect : effects)
        {
            if (isMakoControlledEffect(effect))
                controlled.push_back(effect);
        }
        return controlled;
    }

    inline std::vector<std::string> customEffects(const std::vector<std::string>& effects)
    {
        std::vector<std::string> custom;
        for (const auto& effect : effects)
        {
            if (!isMakoControlledEffect(effect))
                custom.push_back(effect);
        }
        return custom;
    }

    inline bool validMakoEffectSelection(const std::vector<std::string>& effects)
    {
        int lastCategory = -1;
        for (const auto& effect : effects)
        {
            int category = -1;
            if (effect == "fxaa" || effect == "smaa")
                category = 0;
            else if (effect == "makoVibrance" || effect == "makoCurves" || effect == "makoDeband"
                     || effect == "makoTechnicolor" || effect == "makoSepia"
                     || effect == "makoMonochrome" || effect == "makoVignette")
                category = 1;
            else if (effect == "cas" || effect == "dls")
                category = 2;
            else
                continue;

            if (category <= lastCategory)
                return false;
            lastCategory = category;
        }
        return true;
    }

    inline bool canChangeEffectSelectionLive(const std::vector<std::string>& active,
                                             const std::vector<std::string>& requested)
    {
        return validMakoEffectSelection(requested) && customEffects(active) == customEffects(requested);
    }

    inline std::string effectGraphKey(const std::vector<std::string>& effects)
    {
        std::string key;
        for (const auto& effect : effects)
        {
            if (!key.empty())
                key += ':';
            key += effect;
        }
        return key;
    }
} // namespace vkBasalt

#endif // LIVE_EFFECTS_HPP_INCLUDED
