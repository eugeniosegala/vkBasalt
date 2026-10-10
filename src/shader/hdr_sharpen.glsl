// HDR sharpeners use the same bounded, sRGB-like signal range as the SDR
// filters. PQ code values are not interchangeable with SDR code values:
// applying SDR contrast/halo thresholds to PQ exaggerates bright edges.
layout(constant_id = 0) const bool hdrSharpening = false;

#include "hdr_sharpen_curve.glsl"

vec3 sharpenPq(vec3 code, float range)
{
    code = clamp(code * range, 0.0, 5.296338754);
    bvec3 toe = lessThanEqual(code, vec3(0.04045));
    if (!any(toe)) return sharpenPqCurve(code);
    // Keep the exact dark linear segment; the common bright segment avoids powers.
    vec3 p = pow(code * (203.0 / (12.92 * 10000.0)), vec3(2610.0 / 16384.0));
    vec3 encoded = pow((vec3(3424.0 / 4096.0) + (2413.0 / 128.0) * p) /
        (vec3(1.0) + (2392.0 / 128.0) * p), vec3(2523.0 / 32.0));
    if (all(toe)) return encoded;
    return mix(sharpenPqCurve(code), encoded, toe);
}

// Shared local normalization keeps HDR highlights above reference white rather
// than clipping them into SDR. A single scale preserves the RGB proportions.
float sharpenRange(vec3 maximum)
{
    return max(1.0, max(maximum.r, max(maximum.g, maximum.b)));
}
