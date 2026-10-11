// Evaluate HDR neighbourhoods in the SDR filter's midrange, where a bright
// local maximum is not mistaken for clipped white. A peak of 0.8 still
// suppressed CAS/DLS detail relative to SDR even at maximum strength.
// One reversible RGB-wide scale preserves hue and absolute HDR brightness;
// values already below midrange retain the original SDR contrast/noise math.
float sharpenRange(vec3 maximum)
{
    return max(1.0f, 2.0f * max(maximum.r, max(maximum.g, maximum.b)));
}
