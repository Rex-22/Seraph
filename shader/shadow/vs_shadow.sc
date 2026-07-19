$input a_position

#include "../common.sh"

// Depth-only shadow caster: transform to the light's clip space. bgfx builds
// u_modelViewProj from the shadow view's light view+proj and the per-mesh model.
void main()
{
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
}
