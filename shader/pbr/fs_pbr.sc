$input v_color0, v_texcoord0, v_wpos, v_normal, v_tangent

#include "../common.sh"
#include "../lights.sh"

SAMPLER2D(s_albedo,    0);
SAMPLER2D(s_normal,    1);
SAMPLER2D(s_metalRough, 2);
SAMPLER2D(s_ao,        3);
SAMPLER2D(s_emissive,  4);

// Image-based lighting, bound per-submesh by the renderer (Renderer::SetEnvironment).
SAMPLERCUBE(s_texCubeIrr, 5); // irradiance (diffuse ambient)
SAMPLERCUBE(s_texCube,    6); // prefiltered radiance (specular ambient), mipped
SAMPLER2D(s_brdfLUT,      7); // split-sum BRDF integration LUT (RG)

// Directional (sun) shadow map, bound per-submesh by the renderer.
SAMPLER2DSHADOW(s_shadowMap, 8);

uniform vec4 u_baseColorFactor;   // rgb base color, a alpha
uniform vec4 u_metallicFactor;    // x
uniform vec4 u_roughnessFactor;   // x
uniform vec4 u_emissiveFactor;    // rgb
uniform vec4 u_normalScale;       // x
uniform vec4 u_occlusionStrength; // x
uniform vec4 u_iblParams;         // x intensity, y rotationYaw, z radianceMips, w active
uniform mat4 u_shadowMtx;         // biased light view-proj (world -> shadow UV+depth)
uniform vec4 u_shadowParams;      // x texelSize, y bias, z active, w normalOffset

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

#define SHADOW_PCF_SAMPLES 16
#define SHADOW_PCF_RADIUS   2.5 // filter radius in shadow-map texels

// Golden-angle Vogel disk sample in [-1,1]^2, rotated by phi (Jimenez 2014).
vec2 VogelDiskSample(int i, int count, float phi)
{
	float goldenAngle = 2.39996323;
	float r = sqrt((float(i) + 0.5) / float(count));
	float theta = float(i) * goldenAngle + phi;
	return vec2(r * cos(theta), r * sin(theta));
}

// Interleaved gradient noise -> per-pixel rotation angle, turning PCF banding
// into (cheaper-to-hide) noise.
float ShadowNoise(vec2 xy)
{
	return fract(52.9829189 * fract(0.06711056 * xy.x + 0.00583715 * xy.y));
}

// Sun shadow visibility [0,1] for a world-space fragment. 1.0 (fully lit) when
// shadows are inactive or the fragment falls outside the shadow map.
//
// Anti-acne is slope-scaled: the depth bias (and optional normal offset) scale
// with tan(angle between N and the light), so a surface facing the sun gets ~0
// bias — keeping the contact shadow attached (no peter-panning where geometry
// sits flush) — while grazing surfaces, where acne appears, get more. Filtered
// with a rotated Vogel disk over the hardware compare sampler.
float SampleSunShadow(vec3 wpos, vec3 N, float NoL)
{
	if (u_shadowParams.z < 0.5)
		return 1.0;

	float NoLc = clamp(NoL, 0.0, 1.0);
	float slope = clamp(sqrt(1.0 - NoLc * NoLc) / max(NoLc, 0.1), 0.0, 4.0);

	vec3 p = wpos + N * (u_shadowParams.w * slope);
	vec4 sc = mul(u_shadowMtx, vec4(p, 1.0));
	vec3 tc = sc.xyz / sc.w;

	if (any(greaterThan(tc.xy, vec2_splat(1.0))) ||
	    any(lessThan(tc.xy, vec2_splat(0.0))))
		return 1.0;

	// Rotate per shadow-map texel (portable; avoids gl_FragCoord, which bgfx's
	// cross-compiler does not expose). noiseCoord = tc.xy / texelSize.
	float phi = ShadowNoise(tc.xy / u_shadowParams.x) * 6.2831853;
	float offsetScale = u_shadowParams.x * SHADOW_PCF_RADIUS;
	float depth = tc.z - u_shadowParams.y * (1.0 + slope);

	float sum = 0.0;
	for (int i = 0; i < SHADOW_PCF_SAMPLES; ++i)
	{
		vec2 off = VogelDiskSample(i, SHADOW_PCF_SAMPLES, phi) * offsetScale;
		sum += shadow2D(s_shadowMap, vec3(tc.xy + off, depth));
	}
	return sum / float(SHADOW_PCF_SAMPLES);
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

	// --- Ambient: image-based lighting when bound, else flat term ------------
	vec3 ambient;
	if (u_iblParams.w > 0.5)
	{
		// Rotate sample dirs by the environment yaw (matches the skybox).
		float sy = sin(u_iblParams.y);
		float cy = cos(u_iblParams.y);
		vec3 nR = vec3(cy * N.x + sy * N.z, N.y, -sy * N.x + cy * N.z);
		vec3 R  = reflect(-V, N);
		vec3 rR = vec3(cy * R.x + sy * R.z, R.y, -sy * R.x + cy * R.z);

		// Diffuse irradiance (cosine-convolved cube).
		vec3 irradiance = toLinear(textureCube(s_texCubeIrr, nR).xyz);
		vec3 iblDiffuse = irradiance * diffuseColor;

		// Specular: prefiltered radiance * split-sum (F0 * scale + bias).
		float mip = roughness * (u_iblParams.z - 1.0);
		rR = fixCubeLookup(rR, mip, 256.0);
		vec3 prefiltered = toLinear(textureCubeLod(s_texCube, rR, mip).xyz);
		vec2 envBrdf = texture2D(s_brdfLUT, vec2(NoV, roughness)).xy;
		vec3 iblSpecular = prefiltered * (f0 * envBrdf.x + envBrdf.y);

		ambient = (iblDiffuse + iblSpecular) * ao * u_iblParams.x;
	}
	else
	{
		ambient = u_ambient.rgb * diffuseColor * ao;
	}
	vec3 color = ambient;

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
		// Directional lights (the sun) are shadowed by the sun shadow map.
		float shadow = (type == 0) ? SampleSunShadow(v_wpos, N, NoL) : 1.0;
		color += (diffuse + specular) * radiance * NoL * shadow;
	}

	// --- Emissive ------------------------------------------------------------
	color += toLinear(texture2D(s_emissive, v_texcoord0)).rgb * u_emissiveFactor.rgb;

	// Linear HDR out; the tonemap resolve applies exposure + gamma.
	gl_FragColor = vec4(color, baseTex.a * u_baseColorFactor.a);
}
