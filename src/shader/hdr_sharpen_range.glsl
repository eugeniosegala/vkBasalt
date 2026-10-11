// Shared local normalization keeps HDR detail above reference white without
// clipping it into SDR. Reserve 20% of the filter range for bright edges: a
// local maximum at 1.0 falsely looks clipped to both CAS and DLS and suppresses
// their sharpening. At 0.8 the existing highlight guards still attenuate it.
// Reverse this same RGB-wide scale before output; dim detail is unchanged.
float sharpenRange(vec3 maximum)
{
    return max(1.0f, 1.25f * max(maximum.r, max(maximum.g, maximum.b)));
}
