#ifndef LIVE_EFFECTS_HPP_INCLUDED
#define LIVE_EFFECTS_HPP_INCLUDED

#include <string>
#include <vector>

namespace vkBasalt
{
    inline bool isMakoControlledEffect(const std::string& effect)
    {
        return effect == "fxaa" || effect == "smaa" || effect == "cas" || effect == "dls";
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
        const auto controlled = makoControlledEffects(effects);
        if (controlled.size() > 2)
            return false;
        if (controlled.empty())
            return true;
        if (controlled.size() == 1)
            return true;
        return (controlled[0] == "fxaa" || controlled[0] == "smaa")
               && (controlled[1] == "cas" || controlled[1] == "dls");
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
