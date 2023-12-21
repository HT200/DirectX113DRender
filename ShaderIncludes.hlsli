#ifndef __GGP_SHADER_INCLUDES__
#define __GGP_SHADER_INCLUDES__

#define MAX_SPECULAR_EXPONENT 256.0f

#define LIGHT_TYPE_DIRECTIONAL	0
#define LIGHT_TYPE_POINT		1

// A constant Fresnel value for non-metals (glass and plastic have values of about 0.04)
static const float F0_NON_METAL = 0.04f;
// Minimum roughness for when spec distribution function denominator goes to zero
static const float MIN_ROUGHNESS = 0.0000001f; // 6 zeros after decimal
// Handy to have this as a constant
static const float PI = 3.14159265359f;

struct Light
{
	int		Type;
	float3	Direction;
	float	Range;
	float3	Position;
	float	Intensity;
	float3	Color;
	float	SpotFalloff;
	float3	Padding;
};

struct VertexShaderInput
{
	float3 localPosition	: POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
};

struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float3 worldPosition    : POSITION;
	float4 shadowMapPos		: SHADOW_POSITION;
};

struct VertexToPixel_Sky
{
	float4 position			: SV_POSITION;
	float3 sampleDir		: DIRECTION;
};

float3 NormalMapping(Texture2D normalMap, SamplerState basicSampler, float2 uv, float3 normal, float3 tangent)
{
	float3 unpackedNormal = normalMap.Sample(basicSampler, uv).rgb * 2 - 1;
	unpackedNormal = normalize(unpackedNormal); // Don’t forget to normalize!

	// Feel free to adjust/simplify this code to fit with your existing shader(s)
	// Simplifications include not re-normalizing the same vector more than once!
	float3 N = normalize(normal); // Must be normalized here or before
	float3 T = normalize(tangent); // Must be normalized here or before
	T = normalize(T - N * dot(T, N)); // Gram-Schmidt assumes T&N are normalized!
	float3 B = cross(T, N);
	float3x3 TBN = float3x3(T, B, N);

	return normalize(mul(unpackedNormal, TBN));
}

float randomNoise(float2 s, float offset)
{
	return frac(sin(dot(s, float2(12.9898, 78.233))) * 43758.5453123 * offset);
}

float Attenuate(Light light, float3 worldPos)
{
	float dist = distance(light.Position, worldPos);
	float att = saturate(1.0f - (dist * dist / (light.Range * light.Range)));
	return att * att;
}

// Calculate the diffuse amount for this light
float3 Diffuse(float3 normal, float3 dirToLight)
{
	return saturate(dot(normal, dirToLight));
}

float SpecularPhong(float3 normal, float3 dirToLight, float3 toCamera, float roughness)
{
	// Shader conditional to make sure the reflectiveness = 0 when roughness = 1
	return roughness == 1 ? 0.0f : pow(max(dot(toCamera, reflect(-dirToLight, normal)), 0), (1 - roughness) * MAX_SPECULAR_EXPONENT);
}





// ------------- PBR FUNCTIONS

// Lambert diffuse BRDF - Same as the basic lighting diffuse calculation!
// - NOTE: this function assumes the vectors are already NORMALIZED!
float DiffusePBR(float3 normal, float3 dirToLight)
{
	return saturate(dot(normal, dirToLight));
}

// Calculates diffuse amount based on energy conservation
//
// diffuse - Diffuse amount
// F - Fresnel result from microfacet BRDF
// metalness - surface metalness amount

float3 DiffuseEnergyConserve(float3 diffuse, float3 F, float metalness)
{
	return diffuse * (1 - F) * (1 - metalness);
}

// Normal Distribution Function: GGX (Trowbridge-Reitz)
//
// a - Roughness
// h - Half vector: (V + L)/2
// n - Normal
//
// D(h, n, a) = a^2 / pi * ((n dot h)^2 * (a^2 - 1) + 1)^2

float D_GGX(float3 n, float3 h, float roughness)
{
	// Pre-calculations
	float NdotH = saturate(dot(n, h));
	float NdotH2 = NdotH * NdotH;
	float a = roughness * roughness;
	float a2 = max(a * a, MIN_ROUGHNESS); // Applied after remap!
	// ((n dot h)^2 * (a^2 - 1) + 1)
	// Can go to zero if roughness is 0 and NdotH is 1; MIN_ROUGHNESS helps here
	float denomToSquare = NdotH2 * (a2 - 1) + 1;
	// Final value
	return a2 / (PI * denomToSquare * denomToSquare);
}

// Fresnel term - Schlick approx.
//
// v - View vector
// h - Half vector
// f0 - Value when l = n
//
// F(v,h,f0) = f0 + (1-f0)(1 - (v dot h))^5

float3 F_Schlick(float3 v, float3 h, float3 f0)
{
	// Pre-calculations
	float VdotH = saturate(dot(v, h));
	// Final value
	return f0 + (1 - f0) * pow(1 - VdotH, 5);
}

// Geometric Shadowing - Schlick-GGX
// - k is remapped to a / 2, roughness remapped to (r+1)/2 before squaring!
//
// n - Normal
// v - View vector
//
// G_Schlick(n,v,a) = (n dot v) / ((n dot v) * (1 - k) * k)
//
// Full G(n,v,l,a) term = G_SchlickGGX(n,v,a) * G_SchlickGGX(n,l,a)

float G_SchlickGGX(float3 n, float3 v, float roughness)
{
	// End result of remapping:
	float k = pow(roughness + 1, 2) / 8.0f;
	float NdotV = saturate(dot(n, v));
	// Final value
	// Note: Numerator should be NdotV (or NdotL depending on parameters).
	// However, these are also in the BRDF's denominator, so they'll cancel!
	// We're leaving them out here AND in the BRDF function as the
	// dot products can get VERY small and cause rounding errors.
	return 1 / (NdotV * (1 - k) + k);
}

// Cook-Torrance Microfacet BRDF (Specular)
//
// f(l,v) = D(h)F(v,h)G(l,v,h) / 4(n dot l)(n dot v)
// - parts of the denominator are canceled out by numerator (see below)
//
// D() - Normal Distribution Function - Trowbridge-Reitz (GGX)
// F() - Fresnel - Schlick approx
// G() - Geometric Shadowing - Schlick-GGX
float3 MicrofacetBRDF(float3 n, float3 l, float3 v, float roughness, float3 f0, out float3 F_out)
{
	// Other vectors
	float3 h = normalize(v + l);

	// Run numerator functions
	float  D = D_GGX(n, h, roughness);
	float3 F = F_Schlick(v, h, f0);
	float  G = G_SchlickGGX(n, v, roughness) * G_SchlickGGX(n, l, roughness);

	// Pass F out of the function for diffuse balance
	F_out = F;

	// Final specular formula
	// Note: The denominator SHOULD contain (NdotV)(NdotL), but they'd be
	// canceled out by our G() term.  As such, they have been removed
	// from BOTH places to prevent floating point rounding errors.
	float3 specularResult = (D * F * G) / 4;

	// One last non-obvious requirement: According to the rendering equation,
	// specular must have the same NdotL applied as diffuse!  We'll apply
	// that here so that minimal changes are required elsewhere.
	return specularResult * max(dot(n, l), 0);
}





// ------------- LIGHTING FUNCTIONS

float3 DirLight(Light light, float3 normal, float3 worldPos, float3 camPos, float roughness, float metalness, float3 surfaceColor, float specularScale)
{
	// Get normalize direction to the light
	float3 nLight = normalize(-light.Direction);

	// Calculate the light amount
	float3 diff = DiffusePBR(normal, nLight);

	// Get specular reflection
	float3 F;
	float3 spec = MicrofacetBRDF(normal, nLight, normalize(camPos - worldPos), roughness, specularScale, F);

	// Calculate diffuse with energy conservation
	float3 balancedDiff = DiffuseEnergyConserve(diff, spec, metalness);

	// Calculate final pixel color
	return (balancedDiff * surfaceColor + spec) * light.Intensity * light.Color;
}

float3 PointLight(Light light, float3 normal, float3 worldPos, float3 camPos, float roughness, float metalness, float3 surfaceColor, float specularScale)
{
	// Calc light direction
	float3 nLight = normalize(light.Position - worldPos);

	// Calculate attenuation
	float atten = Attenuate(light, worldPos);

	// Calculate the light amount
	float diff = DiffusePBR(normal, nLight);

	// Get specular reflection with phong material
	float3 F;
	float3 spec = MicrofacetBRDF(normal, nLight, normalize(camPos - worldPos), roughness, specularScale, F);

	// Calculate diffuse with energy conservation
	float3 balancedDiff = DiffuseEnergyConserve(diff, spec, metalness);

	// Calculate final pixel color
	return (balancedDiff * surfaceColor + spec) * atten * light.Intensity * light.Color;
}

#endif