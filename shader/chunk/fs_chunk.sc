$input v_texcoord0, v_texcoord1, v_ao

#include "../common.sh"

SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texOverlay, 1);

void main()
{
    // Sample base texture
    vec4 baseColor = texture2D(s_texColor, v_texcoord0);

    // Check if this quad has an overlay
    // Overlay UV is (0,0) when there's no overlay, which we want to skip
    // Check if UV is significantly different from (0,0)
    float overlayUVLength = length(v_texcoord1);
    bool hasOverlay = overlayUVLength > 0.01;

    vec4 finalColor = baseColor;

    if (hasOverlay) {
        // Sample overlay texture and blend with base
        vec4 overlayColor = texture2D(s_texOverlay, v_texcoord1);

        // Blend overlay on top of base using alpha blending
        finalColor = vec4(
            mix(baseColor.rgb, overlayColor.rgb, overlayColor.a),
            max(baseColor.a, overlayColor.a)
        );
    }

    // Apply ambient occlusion
    vec3 aoColor = finalColor.rgb * v_ao;

    gl_FragColor = vec4(aoColor, finalColor.a);
}