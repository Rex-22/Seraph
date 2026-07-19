$input v_color0, v_texcoord0, v_wpos, v_normal, v_tangent

#include "../common.sh"
#include "../lights.sh"

SAMPLER2D(s_albedo,    0);
SAMPLER2D(s_normal,    1);
SAMPLER2D(s_metalRough, 2);
SAMPLER2D(s_ao,        3);
SAMPLER2D(s_emissive,  4);

uniform vec4 u_baseColorFactor;   // rgb base color, a alpha
uniform vec4 u_metallicFactor;    // x
uniform vec4 u_roughnessFactor;   // x
uniform vec4 u_emissiveFactor;    // rgb
uniform vec4 u_normalScale;       // x
uniform vec4 u_occlusionStrength; // x

#define PBR_PI 3.1415926535897932

// GGX/Trowbridge-Reitz normal distribution.
float D_GGX(float NoH, float a)
{
	float a2 = a * a;
	float d = (NoH * NoH) * (a2 - 1.0) + 1.0;
	return a2 / max(PBR_PI * d * d, 1e-8);
}

// Height-correlated Smith visibility term (folds in the 1/(4 NoL NoV) denom).
float V_SmithGGXCorrelated(float NoV, float NoL, float a)
{
	float a2 = a * a;
	float ggxV = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
	float ggxL = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
	return 0.5 / max(ggxV + ggxL, 1e-5);
}

vec3 F_Schlick(float VoH, vec3 f0)
{
	float f = pow(1.0 - VoH, 5.0);
	return f0 + (vec3_splat(1.0) - f0) * f;
}

void main()
{
	// --- Material inputs (textures default to white; factors dominate) -------
	vec4 baseTex = toLinear(texture2D(s_albedo, v_texcoord0));
	vec3 albedo  = baseTex.rgb * u_baseColorFactor.rgb * v_color0.rgb;

	// glTF metal-rough packing: G = roughness, B = metallic.
	vec3 orm = texture2D(s_metalRough, v_texcoord0).rgb;
	float metallic  = clamp(u_metallicFactor.x  * orm.b, 0.0, 1.0);
	float roughness = clamp(u_roughnessFactor.x * orm.g, 0.04, 1.0);

	float ao = texture2D(s_ao, v_texcoord0).r;
	ao = mix(1.0, ao, u_occlusionStrength.x);

	// --- Normal (tangent-space normal map; flat-normal default) --------------
	vec3 N = normalize(v_normal);
	vec3 T = normalize(v_tangent.xyz);
	vec3 B = cross(N, T) * v_tangent.w;
	vec3 nTS = texture2D(s_normal, v_texcoord0).xyz * 2.0 - 1.0;
	nTS.xy *= u_normalScale.x;
	N = normalize(nTS.x * T + nTS.y * B + nTS.z * N);

	vec3 V = normalize(u_cameraPos.xyz - v_wpos);
	float NoV = max(dot(N, V), 1e-4);

	vec3 f0 = mix(vec3_splat(0.04), albedo, metallic);
	vec3 diffuseColor = albedo * (1.0 - metallic);
	float a = roughness * roughness;

	// --- Ambient (flat, occluded) --------------------------------------------
	vec3 color = u_ambient.rgb * diffuseColor * ao;

	// --- Direct punctual lights ----------------------------------------------
	int count = int(u_lightCount.x);
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		if (i >= count)
			break;

		vec4 posRange = u_lightPosRange[i];
		vec4 colInt   = u_lightColorIntensity[i];
		vec4 dirType  = u_lightDirType[i];
		vec4 spot     = u_lightSpot[i];
		int type = int(dirType.w);

		vec3 L;
		float atten = 1.0;
		if (type == 0)
		{
			// Directional: dir is the direction of travel; L points to the light.
			L = normalize(-dirType.xyz);
		}
		else
		{
			vec3 toLight = posRange.xyz - v_wpos;
			float dist = length(toLight);
			L = toLight / max(dist, 1e-4);

			atten = 1.0 / max(dist * dist, 1e-4);
			float range = posRange.w;
			if (range > 0.0)
			{
				float t = dist / range;
				float win = clamp(1.0 - t * t * t * t, 0.0, 1.0);
				atten *= win * win;
			}
			if (type == 2)
			{
				// Cone falloff: cosAngle between spot axis and light->fragment.
				float cosAngle = dot(dirType.xyz, -L);
				float sc = clamp(cosAngle * spot.x + spot.y, 0.0, 1.0);
				atten *= sc * sc;
			}
		}

		float NoL = max(dot(N, L), 0.0);
		if (NoL <= 0.0)
			continue;

		vec3 H = normalize(V + L);
		float NoH = max(dot(N, H), 0.0);
		float VoH = max(dot(V, H), 0.0);

		float D = D_GGX(NoH, a);
		float Vis = V_SmithGGXCorrelated(NoV, NoL, a);
		vec3 F = F_Schlick(VoH, f0);

		vec3 specular = D * Vis * F;
		vec3 diffuse = (vec3_splat(1.0) - F) * diffuseColor / PBR_PI;

		vec3 radiance = colInt.rgb * colInt.w * atten;
		color += (diffuse + specular) * radiance * NoL;
	}

	// --- Emissive ------------------------------------------------------------
	color += toLinear(texture2D(s_emissive, v_texcoord0)).rgb * u_emissiveFactor.rgb;

	// Linear HDR out; the tonemap resolve applies exposure + gamma.
	gl_FragColor = vec4(color, baseTex.a * u_baseColorFactor.a);
}
