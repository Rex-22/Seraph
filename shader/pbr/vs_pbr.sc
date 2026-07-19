$input a_position, a_color0, a_texcoord0, a_normal, a_tangent
$output v_color0, v_texcoord0, v_wpos, v_normal, v_tangent

#include "../common.sh"

void main()
{
	vec4 wpos = mul(u_model[0], vec4(a_position, 1.0));
	v_wpos = wpos.xyz;
	gl_Position = mul(u_viewProj, wpos);

	// cofactor(u_model) is the inverse-transpose (correct for non-uniform scale);
	// tangents transform by the plain model matrix (w = 0 drops translation).
	v_normal = normalize(mul(cofactor(u_model[0]), a_normal));
	v_tangent.xyz = normalize(mul(u_model[0], vec4(a_tangent.xyz, 0.0)).xyz);
	v_tangent.w = a_tangent.w;

	v_texcoord0 = a_texcoord0;
	v_color0 = a_color0;
}
