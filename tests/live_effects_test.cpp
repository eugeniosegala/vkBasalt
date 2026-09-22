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
    assert(!vkBasalt::validMakoEffectSelection(Effects{"cas", "fxaa"}));
    assert(!vkBasalt::validMakoEffectSelection(Effects{"fxaa", "smaa"}));
    assert(!vkBasalt::validMakoEffectSelection(Effects{"cas", "dls"}));
    assert(!vkBasalt::validMakoEffectSelection(Effects{"makoCurves", "makoVibrance"}));
    assert(!vkBasalt::validMakoEffectSelection(Effects{"cas", "makoDeband"}));

    assert(vkBasalt::canChangeEffectSelectionLive(Effects{"cas"}, Effects{"smaa", "dls"}));
    assert(vkBasalt::canChangeEffectSelectionLive(Effects{"cas"}, Effects{"makoVibrance", "cas"}));
    assert(vkBasalt::canChangeEffectSelectionLive(Effects{"makoVibrance", "cas"}, Effects{"makoCurves", "dls"}));
    assert(vkBasalt::canChangeEffectSelectionLive(Effects{"makoCurves"}, Effects{"makoDeband"}));
    assert(vkBasalt::canChangeEffectSelectionLive(Effects{"Custom@Tone", "cas"}, Effects{"Custom@Tone", "fxaa", "dls"}));
    assert(!vkBasalt::canChangeEffectSelectionLive(Effects{"Custom@Tone", "cas"}, Effects{"Other@Tone", "fxaa", "dls"}));
    assert(!vkBasalt::canChangeEffectSelectionLive(Effects{"cas"}, Effects{"cas", "fxaa"}));

    assert(vkBasalt::effectGraphKey(Effects{}) == "");
    assert(vkBasalt::effectGraphKey(Effects{"fxaa", "cas"}) == "fxaa:cas");
    return 0;
}
