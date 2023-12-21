#include "Camera.h"
#include "Input.h"
#include <iostream> 
using namespace std;

using namespace DirectX;

Camera::Camera(DirectX::XMFLOAT3 position, float aspectRatio, float fieldOfView, float movementSpeed, float mouseLookSpeed, float nearClip, float farClip):
	fieldOfView(fieldOfView),
	movementSpeed(movementSpeed),
	mouseLookSpeed(mouseLookSpeed),
	nearClip(nearClip),
	farClip(farClip)
{
    transform.SetPosition(position);
    UpdateViewMatrix();
    UpdateProjectionMatrix(aspectRatio);
}

Camera::Camera(float x, float y, float z, float aspectRatio, float fieldOfView, float movementSpeed, float mouseLookSpeed, float nearClip, float farClip) :
	fieldOfView(fieldOfView),
	movementSpeed(movementSpeed),
	mouseLookSpeed(mouseLookSpeed),
	nearClip(nearClip),
	farClip(farClip)
{
    transform.SetPosition(x, y, z);
    UpdateViewMatrix();
    UpdateProjectionMatrix(aspectRatio);
}

DirectX::XMFLOAT4X4 Camera::GetView() { return viewMatrix; }
DirectX::XMFLOAT4X4 Camera::GetProjection() { return projMatrix; }
Transform* Camera::GetTransform() { return &transform; }
float Camera::GetFieldOfView() { return fieldOfView; }

void Camera::UpdateProjectionMatrix(float aspectRatio)
{
	XMMATRIX projection = XMMatrixPerspectiveFovLH(fieldOfView,	aspectRatio, nearClip, farClip);
	XMStoreFloat4x4(&projMatrix, projection);
}

void Camera::UpdateViewMatrix()
{
	XMFLOAT3 position = transform.GetPosition();
	XMFLOAT3 forward = transform.GetForward();
	XMMATRIX view = XMMatrixLookToLH(XMLoadFloat3(&position), XMLoadFloat3(&forward), XMVectorSet(0, 1, 0, 0));
	XMStoreFloat4x4(&viewMatrix, view);
}

void Camera::Update(float dt)
{
	float speed = dt * movementSpeed;
	Input& input = Input::GetInstance();

	if (input.KeyDown('W')) { transform.MoveRelative(0, 0, speed); }
	if (input.KeyDown('S')) { transform.MoveRelative(0, 0, -speed); }
	if (input.KeyDown('D')) { transform.MoveRelative(speed, 0, 0); }
	if (input.KeyDown('A')) { transform.MoveRelative(-speed, 0, 0); }
	if (input.KeyDown(' ')) { transform.MoveAbsolute(0, speed, 0); }
	if (input.KeyDown('X')) { transform.MoveAbsolute(0, -speed, 0); }

	if (input.MouseLeftDown())
	{
		float cursorMovementX = mouseLookSpeed * input.GetMouseXDelta();
		float cursorMovementY = mouseLookSpeed * input.GetMouseYDelta();
		transform.Rotate(cursorMovementY, cursorMovementX, 0);

		// Clamping
		XMFLOAT3 rotation = transform.GetPitchYawRoll();
		if (rotation.x > XM_PIDIV2) rotation.x = XM_PIDIV2;
		if (rotation.x < -XM_PIDIV2) rotation.x = -XM_PIDIV2;
		transform.SetRotation(rotation);
	}

	UpdateViewMatrix();
}
