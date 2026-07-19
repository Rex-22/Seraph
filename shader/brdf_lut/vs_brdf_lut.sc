$input a_position, a_texcoord0
$output v_texcoord0

#include "../common.sh"

// Fullscreen pass: positions arrive already in clip space (see
// Renderer::DrawFullscreen). v_texcoord0 = (NdotV, roughness) across the target.
void main()
{
	gl_Position = vec4(a_position, 1.0);
	v_texcoord0 = a_texcoord0;
}
