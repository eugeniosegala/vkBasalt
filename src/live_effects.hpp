#ifndef LIVE_EFFECTS_HPP_INCLUDED
#define LIVE_EFFECTS_HPP_INCLUDED

#include <algorithm>
#include <string>
#include <vector>

namespace vkBasalt
{
    inline bool isMakoShaderEffect(const std::string& effect)
    {
        return effect == "makoVibrance" || effect == "makoCurves" || effect == "makoDeband"
               || effect == "makoTechnicolor" || effect == "makoSepia"
               || effect == "makoMonochrome" || effect == "makoVignette"
               || effect == "makoHDRLook" || effect == "makoColourfulness"
               || effect == "makoTechnicolor2" || effect == "makoDPX"
               || effect == "makoBleachBypass" || effect == "makoNoir"
               || effect == "makoFilmGrain" || effect == "makoCartoon"
               || effect == "makoNostalgia" || effect == "makoChromaticAberration"
               || effect == "makoClarity" || effect == "makoLevelsPlus";
    }

    inline bool isMakoControlledEffect(const std::string& effect)
    {
        return effect == "fxaa" || effect == "smaa" || effect == "cas" || effect == "dls"
               || isMakoShaderEffect(effect);
    }

    inline bool validMakoEffectSelection(const std::vector<std::string>& effects)
    {
        int lastCategory = -1;
        std::vector<std::string> seenShaders;
        for (const auto& effect : effects)
        {
            int category = -1;
            if (effect == "fxaa" || effect == "smaa")
                category = 0;
            else if (isMakoShaderEffect(effect))
                category = 1;
            else if (effect == "cas" || effect == "dls")
                category = 2;
            else
                continue;

            if (category < lastCategory || (category == lastCategory && category != 1))
                return false;
            if (category == 1)
            {
                if (std::find(seenShaders.begin(), seenShaders.end(), effect) != seenShaders.end())
                    return false;
                seenShaders.push_back(effect);
            }
            lastCategory = category;
        }
        return true;
    }

    inline bool canChangeEffectSelectionLive(const std::vector<std::string>& requested)
    {
        if (!validMakoEffectSelection(requested))
            return false;
        std::vector<std::string> seen;
        for (const auto& effect : requested)
        {
            if (effect.empty() || std::find(seen.begin(), seen.end(), effect) != seen.end())
                return false;
            seen.push_back(effect);
        }
        return true;
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
