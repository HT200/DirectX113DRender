#include "ShaderIncludes.hlsli"

cbuffer ExternalData : register(b0)
{
	matrix view;
	matrix projection;
}

VertexToPixel_Sky main(VertexShaderInput input)
{
	VertexToPixel_Sky output;

	// Create a copy of the view matrix and set the translation portion of that copy to all zeros
	matrix viewNoTranslation = view;
	viewNoTranslation._14 = 0;
	viewNoTranslation._24 = 0;
	viewNoTranslation._34 = 0;

	// Apply projection & updated view to the input position
	matrix vProj = mul(projection, viewNoTranslation);
	output.position = mul(vProj, float4(input.localPosition, 1.0f));

	// Ensure that the output depth of each vertex will be exactly 1.0 after the shader
	output.position.z = output.position.w;

	// Assign input position to sample direction
	output.sampleDir = input.localPosition;

	return output;
}