$input v_dir

#include "../common.sh"

SAMPLERCUBE(s_skyCube, 0);

// x = intensity, y = rotation yaw (radians), z = mip LOD to sample, w unused.
uniform vec4 u_skyParams;

void main()
{
	vec3 dir = normalize(v_dir);

	// Yaw-rotate the sample direction so the environment can be oriented.
	float s = sin(u_skyParams.y);
	float c = cos(u_skyParams.y);
	vec3 rd;
	rd.x =  c * dir.x + s * dir.z;
	rd.y =  dir.y;
	rd.z = -s * dir.x + c * dir.z;

	// The baked radiance cubes store gamma-encoded values (matches bgfx 18-ibl);
	// decode to linear here since the scene target is linear HDR and the tonemap
	// resolve owns the single gamma encode.
	vec3 color = toLinear(textureCubeLod(s_skyCube, rd, u_skyParams.z)).rgb;
	color *= u_skyParams.x;

	gl_FragColor = vec4(color, 1.0);
}
