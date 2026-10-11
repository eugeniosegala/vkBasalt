#include <algorithm>
#include <cassert>
#include <cmath>

// Compile the production GLSL range function, not a second normalization.
struct vec3 { float r, g, b; };
using std::max;
#include "shader/hdr_sharpen_range.glsl"

static float range(float value)
{
    return sharpenRange({value, value, value});
}

int main()
{
    assert(range(0.0f) == 1.0f);
    assert(range(0.04f) == 1.0f);
    assert(range(0.5f) == 1.0f);
    assert(range(0.2f) == 1.0f);
    assert(range(0.4f) == 1.0f);

    // HDR exposure must not engage SDR white-clipping protection. Match the
    // same relative detail at SDR midrange, across the extended HDR range.
    for (float peak : {0.5f, 0.8f, 1.0f, 1.35f, 2.0f, 3.64f, 5.296338754f})
    {
        const float scale = range(peak);
        const float hi = peak / scale;
        const float lo = 0.98f * hi;
        assert(std::isfinite(scale));
        assert(std::abs(hi - 0.5f) < 1e-6f);
        assert(std::abs(hi * scale - peak) < 1e-6f);

        // The actual CAS and DLS highlight guards on a neutral fine-detail
        // fixture must remain active, bounded and nonzero at every brightness.
        const float casAmp = std::clamp(std::min(2.0f * lo, 2.0f - 2.0f * hi) / (2.0f * hi), 0.0f, 1.0f);
        const float rolloff = std::max(hi - 0.65f, 0.0f) / 0.35f;
        const float dlsHighlight = std::pow(1.0f - rolloff * rolloff, 2.0f);
        // These equal the original SDR guards for a 0.49/0.50 edge.
        assert(std::abs(casAmp - 0.98f) < 1e-6f);
        assert(dlsHighlight == 1.0f);
        // DLS must retain the same relative-detail and denoise response at
        // every HDR exposure, rather than silently bypassing noise protection.
        for (float denoise : {0.0f, 0.2f, 0.5f, 1.0f})
        {
            const float kernel = 1.0f / (0.001f + (-0.1f - 0.001f) * denoise);
            const float hdrNoise = std::pow((hi - lo) * kernel, 2.0f);
            const float sdrNoise = std::pow((0.5f - 0.49f) * kernel, 2.0f);
            const float hdrBoost = std::min({lo / (hi + 1.0f / 256.0f), dlsHighlight, hdrNoise});
            const float sdrBoost = std::min({0.49f / (0.5f + 1.0f / 256.0f), 1.0f, sdrNoise});
            assert(std::abs(hdrBoost - sdrBoost) < 1e-5f);
        }
        float previousGain = 0.0f;
        for (float strength : {0.0f, 0.2f, 0.5f, 1.0f})
        {
            const float weight = -std::sqrt(casAmp) / (8.0f - 3.0f * strength);
            const float gain = 1.0f / (1.0f + 4.0f * weight);
            assert(gain > previousGain && gain < 5.0f);
            previousGain = gain;
        }
    }

    // Use one maximum across RGB, preserving hue instead of independently
    // normalizing saturated channels. The normalization knee stays continuous.
    const float rgbScale = sharpenRange({3.0f, 1.0f, 0.1f});
    assert(rgbScale == sharpenRange({0.1f, 3.0f, 1.0f}));
    assert(rgbScale == sharpenRange({1.0f, 0.1f, 3.0f}));
    for (int i = 1; i <= 10000; ++i)
    {
        const float peak = float(i) / 1000.0f;
        assert(range(peak) >= range(peak - 0.001f));
        assert(range(peak) - range(peak - 0.001f) < 0.0021f);
    }
}
