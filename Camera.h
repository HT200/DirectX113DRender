#pragma once
#include <DirectXMath.h>
#include "Transform.h"

class Camera
{
private:
	Transform transform;

	DirectX::XMFLOAT4X4 viewMatrix;
	DirectX::XMFLOAT4X4 projMatrix;

	float fieldOfView;
	float nearClip;
	float farClip;

	float movementSpeed;
	float mouseLookSpeed;

public:
	Camera(DirectX::XMFLOAT3 position, float aspectRatio, float fieldOfView, float movementSpeed, float mouseLookSpeed, float nearClip = 0.01f, float farClip = 100.0f);
	Camera(float x, float y, float z, float aspectRatio, float fieldOfView, float movementSpeed, float mouseLookSpeed, float nearClip = 0.01f, float farClip = 100.0f);

	DirectX::XMFLOAT4X4 GetView();
	DirectX::XMFLOAT4X4 GetProjection();
	Transform* GetTransform();
	float GetFieldOfView();

	void UpdateProjectionMatrix(float aspectRatio);
	void UpdateViewMatrix();
	void Update(float dt);
};