$input v_texcoord0

#include "../common.sh"

// Karis split-sum BRDF integration. For each texel (x = NdotV, y = roughness)
// we Monte-Carlo integrate the environment BRDF with GGX importance sampling,
// producing (scale, bias) so the PBR shader can reconstruct specular IBL as
// prefiltered * (F0 * scale + bias). Baked once into an RG16F LUT (Render 14).

#define BRDF_PI 3.1415926535897932
#define SAMPLE_COUNT 1024

// Van der Corput radical inverse in base 2, done with integer arithmetic only
// (no uint / bit ops — bgfx's shaderc rejects those in this profile). 16 mirrored
// bits is ample for SAMPLE_COUNT indices. `n - (n/2)*2` is n mod 2 (low bit).
float RadicalInverseVdC(int i)
{
	float result = 0.0;
	float f = 0.5;
	int n = i;
	for (int k = 0; k < 16; ++k)
	{
		result += f * float(n - (n / 2) * 2);
		n = n / 2;
		f *= 0.5;
	}
	return result;
}

vec2 Hammersley(int i, int n)
{
	return vec2(float(i) / float(n), RadicalInverseVdC(i));
}

// GGX importance-sampled half vector in the hemisphere around N=(0,0,1).
vec3 ImportanceSampleGGX(vec2 Xi, float roughness)
{
	float a = roughness * roughness;
	float phi = 2.0 * BRDF_PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
	return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

// Smith geometry with the IBL k = a^2/2 (differs from the direct-light k).
float GeometrySchlickGGX(float NdotV, float roughness)
{
	float a = roughness * roughness;
	float k = (a * a) * 0.5;
	return NdotV / (NdotV * (1.0 - k) + k);
}

void main()
{
	float NdotV = max(v_texcoord0.x, 1e-4);
	float roughness = v_texcoord0.y;

	vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);

	float A = 0.0;
	float B = 0.0;
	for (int i = 0; i < SAMPLE_COUNT; ++i)
	{
		vec2 Xi = Hammersley(i, SAMPLE_COUNT);
		vec3 H = ImportanceSampleGGX(Xi, roughness);
		vec3 L = 2.0 * dot(V, H) * H - V;

		float NoL = max(L.z, 0.0);
		float NoH = max(H.z, 0.0);
		float VoH = max(dot(V, H), 0.0);

		if (NoL > 0.0)
		{
			float G = GeometrySchlickGGX(NoL, roughness) *
			          GeometrySchlickGGX(NdotV, roughness);
			float G_Vis = (G * VoH) / (NoH * NdotV);
			float Fc = pow(1.0 - VoH, 5.0);
			A += (1.0 - Fc) * G_Vis;
			B += Fc * G_Vis;
		}
	}

	gl_FragColor = vec4(A / float(SAMPLE_COUNT), B / float(SAMPLE_COUNT), 0.0, 1.0);
}
