$input v_color0, v_texcoord0

#include "common.sh"

SAMPLER2D(s_texColor,  0);

void main() {
    vec4 color = toLinear(texture2D(s_texColor, v_texcoord0));

    gl_FragColor.xyz = color.xyz;
    gl_FragColor.w = 1.0;
    gl_FragColor = toGamma(gl_FragColor);
}
