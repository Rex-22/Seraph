$input v_color0, v_texcoord0

#include "../common.sh"

SAMPLER2D(s_texColor, 0);

void main()
{
	// Output LINEAR light. The tonemap resolve pass (fs_tonemap) applies exposure,
	// tone mapping, and the final gamma encode — so no toGamma here.
	vec4 color = toLinear(texture2D(s_texColor, v_texcoord0));
    gl_FragColor.xyz = color.xyz * v_color0.xyz;
    gl_FragColor.w = 1.0;
}