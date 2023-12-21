
#include "ShaderIncludes.hlsli"

cbuffer ExternalData : register(b0)
{
	matrix world;
	matrix view;
	matrix projection;
	matrix worldInvTrans;

	matrix lightView;
	matrix lightProjection;
}

// --------------------------------------------------------
// The entry point (main method) for our vertex shader
// 
// - Input is exactly one vertex worth of data (defined by a struct)
// - Output is a single struct of data to pass down the pipeline
// - Named "main" because that's the default the shader compiler looks for
// --------------------------------------------------------
VertexToPixel main(VertexShaderInput input)
{
	// Set up output struct
	VertexToPixel output;

	// Calculate screen position of this vertex
	matrix wvp = mul(projection, mul(view, world));
	output.screenPosition = mul(wvp, float4(input.localPosition, 1.0f));

	output.uv = input.uv;
	output.normal = normalize(mul((float3x3)worldInvTrans, input.normal));
	output.tangent = normalize(mul((float3x3)worldInvTrans, input.tangent));
	output.worldPosition = mul(world, float4(input.localPosition, 1.0f)).xyz;

	matrix shadowWVP = mul(lightProjection, mul(lightView, world));
	output.shadowMapPos = mul(shadowWVP, float4(input.localPosition, 1.0f));

	return output;
}