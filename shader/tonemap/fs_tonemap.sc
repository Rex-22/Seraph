$input v_texcoord0

#include "../common.sh"

// Resolve the HDR (linear) scene color to the display: apply exposure, an
// optional tone-mapping curve, then encode to gamma. Scene/material shaders now
// output LINEAR light; this pass owns the sRGB gamma encode for the final image.
SAMPLER2D(s_hdr, 0);

// x = exposure multiplier, y = operator (0 None, 1 Reinhard, 2 ACES filmic).
uniform vec4 u_tonemapParams;

void main()
{
	vec3 color = texture2D(s_hdr, v_texcoord0).xyz;
	color *= u_tonemapParams.x;

	int op = int(u_tonemapParams.y + 0.5);
	if (op == 1)
		color = color / (color + vec3_splat(1.0)); // Reinhard
	else if (op == 2)
		color = toAcesFilmic(color);               // ACES filmic
	// op == 0: None — no tone curve, just exposure + gamma below.

	gl_FragColor = vec4(toGamma(color), 1.0);
}
