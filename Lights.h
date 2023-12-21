#pragma once
#include <DirectXMath.h>

#define LIGHT_TYPE_DIRECTIONAL  0
#define LIGHT_TYPE_POINT		1
#define LIGHT_TYPE_SPOT			2

struct Light
{
	int Type;						// Which kind of light? 0, 1 or 2 (see above)
	DirectX::XMFLOAT3 Direction;	// Directional and Spot lights need a direction
	float Range;					// Point and Spot lights have a max range for attenuation
	DirectX::XMFLOAT3 Position;		// Point and Spot lights have a position in space
	float Intensity;				// All lights need an intensity
	DirectX::XMFLOAT3 Color;		// All lights need a color
	float SpotFalloff;				// Spot lights need a value to define their “cone” size
	DirectX::XMFLOAT3 Padding;		// Purposefully padding to hit the 16-byte boundary
};