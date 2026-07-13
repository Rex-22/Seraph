$input v_color0, v_texcoord0

#include "bgfx_shader.sh"

// Material parameters (each is a vec4 register; floats read .x):
uniform vec4 u_colorA;  // palette color A
uniform vec4 u_colorB;  // palette color B + bloom tint
uniform vec4 u_tiling;  // x: kaleidoscope petal count / symmetry
uniform vec4 u_glow;    // x: center bloom strength
uniform vec4 u_warp;    // x: swirl amount

void main()
{
    // Work in polar coordinates around the face centre.
    vec2 p = v_texcoord0 - vec2(0.5, 0.5);
    float r = length(p) * 2.0;
    float a = atan2(p.y, p.x);

    // Swirl: twist the angle by the radius.
    a += u_warp.x * 6.2831853 * r;

    // Kaleidoscope petals from angular symmetry.
    float petals = max(u_tiling.x, 1.0);
    float k = 0.5 + 0.5 * sin(a * petals);

    // Concentric rings, phase-shifted by the petal field.
    float rings = 0.5 + 0.5 * sin(r * 18.0 - k * 6.2831853);

    // Blend the two fields into a pattern value.
    float pattern = mix(k, rings, 0.5);

    // Palette mix, with a channel-swizzle "hue rotation" for an iridescent look.
    vec3 col = mix(u_colorA.xyz, u_colorB.xyz, pattern);
    col = mix(col, col.yzx, k * 0.4);

    // Bloom out of the centre.
    float bloom = (1.0 - smoothstep(0.0, 1.0, r)) * u_glow.x;
    col += bloom * u_colorB.xyz;

    // Soft circular vignette toward the face edges.
    col *= smoothstep(1.15, 0.2, r);

    // Modulate by the mesh's vertex colour.
    col *= v_color0.xyz;

    gl_FragColor = vec4(col, 1.0);
}
