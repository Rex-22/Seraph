$input v_texcoord0, v_ao

#include "../common.sh"

SAMPLER2D(s_texColor, 0);

void main()
{
    vec4 texelColor = texture2D(s_texColor, v_texcoord0);

    // Apply ambient occlusion
    vec3 finalColor = texelColor.rgb * v_ao;

    gl_FragColor = vec4(finalColor, texelColor.a);
}