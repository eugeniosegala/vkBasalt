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

    // Custom activation/order stays restart-bound. Project bundled edits onto
    // the running custom chain so a pending custom edit cannot freeze them.
    inline std::vector<std::string> liveEffectSelection(const std::vector<std::string>& active,
                                                         const std::vector<std::string>& requested)
    {
        if (customEffects(active) == customEffects(requested))
            return requested;

        auto live = makoControlledEffects(requested);
        size_t insertionFloor = 0;
        for (size_t i = 0; i < active.size(); ++i)
        {
            if (isMakoControlledEffect(active[i]))
                continue;

            size_t position = live.size();
            // Keep a custom effect before its next surviving bundled neighbour.
            for (size_t next = i + 1; next < active.size(); ++next)
            {
                if (!isMakoControlledEffect(active[next]))
                    continue;
                const auto anchor = std::find(live.begin() + insertionFloor, live.end(), active[next]);
                if (anchor != live.end())
                {
                    position = static_cast<size_t>(anchor - live.begin());
                    break;
                }
            }
            if (position == live.size())
            {
                // If its neighbours were removed, keep it before sharpening.
                const auto sharpening = std::find_if(live.begin() + insertionFloor, live.end(),
                    [](const std::string& effect) { return effect == "cas" || effect == "dls"; });
                position = static_cast<size_t>(sharpening - live.begin());
            }
            live.insert(live.begin() + position, active[i]);
            insertionFloor = position + 1;
        }
        return live;
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

    inline bool shouldRetainEffectGraph(const std::string& graphKey,
                                        const std::string& requestedKey)
    {
        // The empty graph is the keyboard-toggle fallback. Every other graph
        // can be rebuilt after a live selection change, so retaining it would
        // keep effect-owned images and pipelines resident for the lifetime of
        // the swapchain.
        return graphKey.empty() || graphKey == requestedKey;
    }
} // namespace vkBasalt

#endif // LIVE_EFFECTS_HPP_INCLUDED
