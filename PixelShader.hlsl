
#include "ShaderIncludes.hlsli"

cbuffer ExternalData : register(b0)
{
	float3 cameraPosition;
	Light lights[5];
}

Texture2D Albedo						: register(t0);
Texture2D NormalMap						: register(t1);
Texture2D RoughnessMap					: register(t2);
Texture2D MetalnessMap					: register(t3);
Texture2D ShadowMap						: register(t4);
SamplerState BasicSampler				: register(s0);
SamplerComparisonState ShadowSampler	: register(s1);

float4 main(VertexToPixel input) : SV_TARGET
{
	input.normal = NormalMapping(NormalMap, BasicSampler, input.uv, input.normal, input.tangent);

	float roughness = RoughnessMap.Sample(BasicSampler, input.uv).r;
	float metalness = MetalnessMap.Sample(BasicSampler, input.uv).r;
	float3 surfaceColor = pow(Albedo.Sample(BasicSampler, input.uv).rgb, 2.2f);
	float3 outputLight = float3(0, 0, 0);

	// Perform the perspective divide (divide by W) ourselves
	input.shadowMapPos /= input.shadowMapPos.w;

	// Convert the normalized device coordinates to UVs for sampling
	float2 shadowUV = input.shadowMapPos.xy * 0.5f + 0.5f;
	shadowUV.y = 1 - shadowUV.y; // Flip the Y

	// Grab the distances we need: light-to-pixel and closest-surface
	float distToLight = input.shadowMapPos.z;
	
	// Get a ratio of comparison results using SampleCmpLevelZero()
	float shadowAmount = ShadowMap.SampleCmpLevelZero(
		ShadowSampler,
		shadowUV,
		distToLight).r;

	// Specular color determination -----------------
	// Assume albedo texture is actually holding specular color where metalness == 1
	// Note the use of lerp here - metal is generally 0 or 1, but might be in between
	// because of linear texture sampling, so we lerp the specular color to match
	float3 specularColor = lerp(F0_NON_METAL, surfaceColor, metalness);
	float3 lightResult;

	for (int i = 0; i < 5; i++)
	{
		Light light = lights[i];
		light.Direction = normalize(light.Direction);

		switch (lights[i].Type)
		{
		case LIGHT_TYPE_DIRECTIONAL:
			lightResult = DirLight(light, input.normal, input.worldPosition, cameraPosition, roughness, metalness, surfaceColor, specularColor);
			outputLight += lightResult * shadowAmount;
			break;

		case LIGHT_TYPE_POINT:
			outputLight += PointLight(light, input.normal, input.worldPosition, cameraPosition, roughness, metalness, surfaceColor, specularColor);
			break;
		}
	}

	return float4(pow(outputLight, 1.0f / 2.2f), 1);
}