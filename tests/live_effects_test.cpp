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

    assert(vkBasalt::canChangeEffectSelectionLive(Effects{"cas"}, Effects{"smaa", "dls"}));
    assert(vkBasalt::canChangeEffectSelectionLive(Effects{"cas"}, Effects{"makoVibrance", "cas"}));
    assert(vkBasalt::canChangeEffectSelectionLive(Effects{"makoVibrance", "cas"}, Effects{"makoCurves", "dls"}));
    assert(vkBasalt::canChangeEffectSelectionLive(Effects{"makoCurves"}, Effects{"makoDeband"}));
    assert(vkBasalt::canChangeEffectSelectionLive(Effects{"makoDeband"}, Effects{"makoTechnicolor"}));
    assert(vkBasalt::canChangeEffectSelectionLive(Effects{"makoTechnicolor"}, Effects{"makoSepia"}));
    assert(vkBasalt::canChangeEffectSelectionLive(Effects{"makoSepia"}, Effects{"makoMonochrome"}));
    assert(vkBasalt::canChangeEffectSelectionLive(Effects{"makoMonochrome"}, Effects{"makoVignette"}));
    assert(vkBasalt::canChangeEffectSelectionLive(Effects{"makoVignette"}, Effects{"makoHDRLook"}));
    assert(vkBasalt::canChangeEffectSelectionLive(Effects{"makoHDRLook"}, Effects{"makoColourfulness"}));
    assert(vkBasalt::canChangeEffectSelectionLive(Effects{"makoColourfulness"}, Effects{"makoNoir"}));
    assert(vkBasalt::canChangeEffectSelectionLive(Effects{"makoNoir"}, Effects{"makoChromaticAberration"}));
    assert(vkBasalt::canChangeEffectSelectionLive(Effects{"makoVibrance", "cas"}, Effects{"makoHDRLook", "makoClarity", "makoVibrance", "makoLevelsPlus", "cas"}));
    assert(vkBasalt::canChangeEffectSelectionLive(Effects{"Custom@Tone", "cas"}, Effects{"Custom@Tone", "fxaa", "dls"}));
    assert(!vkBasalt::canChangeEffectSelectionLive(Effects{"Custom@Tone", "cas"}, Effects{"Other@Tone", "fxaa", "dls"}));
    assert(!vkBasalt::canChangeEffectSelectionLive(Effects{"cas"}, Effects{"cas", "fxaa"}));

    // RE4: deselecting Border then editing bundled shaders used to freeze the
    // entire chain. Keep Border active until restart while applying each edit.
    const Effects borderAtLaunch{"makoLevelsPlus", "makoVibrance", "CustomBorder", "cas"};
    auto live = vkBasalt::liveEffectSelection(borderAtLaunch, Effects{"makoLevelsPlus", "cas"});
    assert((live == Effects{"makoLevelsPlus", "CustomBorder", "cas"}));
    assert(vkBasalt::canChangeEffectSelectionLive(borderAtLaunch, live));
    live = vkBasalt::liveEffectSelection(live, Effects{"makoHDRLook", "cas"});
    assert((live == Effects{"makoHDRLook", "CustomBorder", "cas"}));
    assert((vkBasalt::liveEffectSelection(live, Effects{}) == Effects{"CustomBorder"}));
    // Adding a custom selection during play must not block bundled additions.
    assert((vkBasalt::liveEffectSelection(Effects{"cas"}, Effects{"CustomBorder", "makoVibrance", "cas"})
            == Effects{"makoVibrance", "cas"}));
    // Keep custom ordering and surviving anchors for advanced mixed chains.
    assert((vkBasalt::liveEffectSelection(Effects{"CustomA", "makoVibrance", "CustomB", "cas"},
                                        Effects{"smaa", "makoVibrance", "dls"})
            == Effects{"smaa", "CustomA", "makoVibrance", "CustomB", "dls"}));
    assert((vkBasalt::liveEffectSelection(Effects{"CustomA", "makoVibrance", "CustomB", "cas"},
                                        Effects{"CustomB", "CustomA", "cas"})
            == Effects{"CustomA", "CustomB", "cas"}));
    assert((vkBasalt::liveEffectSelection(Effects{"CustomA", "cas"}, Effects{"CustomA", "makoVibrance", "dls"})
            == Effects{"CustomA", "makoVibrance", "dls"}));
    assert(!vkBasalt::canChangeEffectSelectionLive(Effects{"CustomA", "cas"},
        vkBasalt::liveEffectSelection(Effects{"CustomA", "cas"}, Effects{"cas", "fxaa"})));

    assert(vkBasalt::effectGraphKey(Effects{}) == "");
    assert(vkBasalt::effectGraphKey(Effects{"fxaa", "cas"}) == "fxaa:cas");
    assert(vkBasalt::shouldRetainEffectGraph("", "fxaa:cas"));
    assert(vkBasalt::shouldRetainEffectGraph("fxaa:cas", "fxaa:cas"));
    assert(!vkBasalt::shouldRetainEffectGraph("smaa:cas", "fxaa:cas"));
    assert(!vkBasalt::shouldRetainEffectGraph("makoVibrance:cas", "fxaa:cas"));
    return 0;
}
