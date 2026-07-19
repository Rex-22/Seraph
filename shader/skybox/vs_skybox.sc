$input a_position, a_texcoord0
$output v_dir

#include "../common.sh"

// World-space view ray per pixel, reconstructed from the fullscreen triangle
// (positions arrive already in clip space; see Renderer::DrawFullscreen). We
// unproject a point on the pixel's ray with the inverse view-projection and
// subtract the camera position — depth-convention agnostic (works under
// reversed-Z). The skybox is rasterized at the far plane so it only fills pixels
// no geometry covered (GEQUAL depth test, far = 0.0 under reversed-Z).
uniform mat4 u_skyInvViewProj;
uniform vec4 u_skyCameraPos; // xyz world-space camera position

void main()
{
	vec4 clip = vec4(a_position.xy, 0.0, 1.0);
	vec4 world = mul(u_skyInvViewProj, clip);
	world.xyz /= world.w;
	v_dir = world.xyz - u_skyCameraPos.xyz;

	gl_Position = vec4(a_position.xy, 0.0, 1.0);
}
