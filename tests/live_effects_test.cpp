#include "live_effects.hpp"

#include <cassert>
#include <string>
#include <vector>

int main()
{
    using Effects = std::vector<std::string>;

    assert(vkBasalt::validMakoEffectSelection(Effects{}));
    assert(vkBasalt::validMakoEffectSelection(Effects{"cas"}));
    assert(vkBasalt::validMakoEffectSelection(Effects{"dls"}));
    assert(vkBasalt::validMakoEffectSelection(Effects{"fxaa"}));
    assert(vkBasalt::validMakoEffectSelection(Effects{"smaa"}));
    assert(vkBasalt::validMakoEffectSelection(Effects{"fxaa", "cas"}));
    assert(vkBasalt::validMakoEffectSelection(Effects{"smaa", "dls"}));
    assert(vkBasalt::validMakoEffectSelection(Effects{"makoVibrance"}));
    assert(vkBasalt::validMakoEffectSelection(Effects{"fxaa", "makoCurves", "dls"}));
    assert(vkBasalt::validMakoEffectSelection(Effects{"makoDeband", "cas"}));
    assert(vkBasalt::validMakoEffectSelection(Effects{"makoTechnicolor"}));
    assert(vkBasalt::validMakoEffectSelection(Effects{"fxaa", "makoSepia", "cas"}));
    assert(vkBasalt::validMakoEffectSelection(Effects{"makoMonochrome", "dls"}));
    assert(vkBasalt::validMakoEffectSelection(Effects{"smaa", "makoVignette"}));
    for (const auto& effect : Effects{
             "makoHDRLook",
             "makoColourfulness",
             "makoTechnicolor2",
             "makoDPX",
             "makoBleachBypass",
             "makoNoir",
             "makoFilmGrain",
             "makoCartoon",
             "makoNostalgia",
             "makoChromaticAberration",
             "makoClarity",
             "makoLevelsPlus",
         })
    {
        assert(vkBasalt::validMakoEffectSelection(Effects{effect}));
        assert(vkBasalt::isMakoControlledEffect(effect));
    }
    assert(!vkBasalt::validMakoEffectSelection(Effects{"cas", "fxaa"}));
    assert(!vkBasalt::validMakoEffectSelection(Effects{"fxaa", "smaa"}));
    assert(!vkBasalt::validMakoEffectSelection(Effects{"cas", "dls"}));
    assert(vkBasalt::validMakoEffectSelection(Effects{"makoCurves", "makoVibrance"}));
    assert(vkBasalt::validMakoEffectSelection(Effects{"fxaa", "makoHDRLook", "makoClarity", "makoVibrance", "makoLevelsPlus", "cas"}));
    assert(!vkBasalt::validMakoEffectSelection(Effects{"makoClarity", "makoClarity"}));
    assert(!vkBasalt::validMakoEffectSelection(Effects{"cas", "makoDeband"}));

    for (const auto& selection : std::vector<Effects>{
        {}, {"CustomBorder"}, {"CustomBorder", "cas"},
        {"fxaa", "CustomA", "makoVibrance", "CustomB", "dls"},
        {"CustomB", "CustomA", "cas"}, {"makoVibrance", "cas"}
    })
        assert(vkBasalt::canChangeEffectSelectionLive(selection));
    assert(!vkBasalt::canChangeEffectSelectionLive(Effects{"cas", "fxaa"}));
    assert(!vkBasalt::canChangeEffectSelectionLive(Effects{"CustomA", "CustomA"}));
    assert(!vkBasalt::canChangeEffectSelectionLive(Effects{""}));

    assert(vkBasalt::effectGraphKey(Effects{}) == "");
    assert(vkBasalt::effectGraphKey(Effects{"fxaa", "cas"}) == "fxaa:cas");
    return 0;
}
