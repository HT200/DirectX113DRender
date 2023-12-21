#include "Transform.h"

using namespace DirectX;

void Transform::SetPosition(float x, float y, float z)
{
	position.x = x;
	position.y = y;
	position.z = z;
	matrixChanged = true;
}

void Transform::SetPosition(DirectX::XMFLOAT3 position)
{
	this->position = position;
	matrixChanged = true;
}

void Transform::SetRotation(float pitch, float yaw, float roll)
{
	pitchYawRoll.x = pitch;
	pitchYawRoll.y = yaw;
	pitchYawRoll.z = roll;
	matrixChanged = true;
	vectorChanged = true;
}

void Transform::SetRotation(DirectX::XMFLOAT3 rotation)
{
	pitchYawRoll = rotation;
	matrixChanged = true;
	vectorChanged = true;
}

void Transform::SetScale(float x, float y, float z)
{
	scale.x = x;
	scale.y = y;
	scale.z = z;
	matrixChanged = true;
}

void Transform::SetScale(DirectX::XMFLOAT3 scale)
{
	this->scale = scale;
	matrixChanged = true;
}

DirectX::XMFLOAT3 Transform::GetPosition() { return position; }
DirectX::XMFLOAT3 Transform::GetPitchYawRoll() { return pitchYawRoll; }
DirectX::XMFLOAT3 Transform::GetScale() { return scale; }
DirectX::XMFLOAT4X4 Transform::GetWorldMatrix() { UpdateMatrices(); return worldMatrix; }
DirectX::XMFLOAT4X4 Transform::GetWorldInverseTransposeMatrix() { UpdateMatrices(); return worldInverseTransposeMatrix; }
DirectX::XMFLOAT3 Transform::GetUp() { UpdateVectors(); return up; }
DirectX::XMFLOAT3 Transform::GetRight() { UpdateVectors(); return right; }
DirectX::XMFLOAT3 Transform::GetForward() { UpdateVectors(); return forward; }

void Transform::MoveAbsolute(float x, float y, float z)
{
	position.x += x;
	position.y += y;
	position.z += z;
	matrixChanged = true;
}

void Transform::MoveAbsolute(DirectX::XMFLOAT3 offset)
{
	position.x += offset.x;
	position.y += offset.y;
	position.z += offset.z;
	matrixChanged = true;
}

void Transform::MoveRelative(float x, float y, float z)
{
	XMVECTOR movement = XMVectorSet(x, y, z, 0);
	XMVECTOR rotation = XMQuaternionRotationRollPitchYawFromVector(XMLoadFloat3(&pitchYawRoll));
	XMVECTOR direction = XMVector3Rotate(movement, rotation);
	XMStoreFloat3(&position, XMLoadFloat3(&position) + direction);
	matrixChanged = true;
}

void Transform::MoveRelative(DirectX::XMFLOAT3 offset)
{
	MoveRelative(offset.x, offset.y, offset.z);
}

void Transform::Rotate(float pitch, float yaw, float roll)
{
	pitchYawRoll.x += pitch;
	pitchYawRoll.y += yaw;
	pitchYawRoll.z += roll;
	matrixChanged = true;
	vectorChanged = true;
}

void Transform::Rotate(DirectX::XMFLOAT3 rotation)
{
	pitchYawRoll.x += rotation.x;
	pitchYawRoll.y += rotation.y;
	pitchYawRoll.z += rotation.z;
	matrixChanged = true;
	vectorChanged = true;
}

void Transform::Scale(float x, float y, float z)
{
	scale.x *= x;
	scale.y *= y;
	scale.z *= z;
	matrixChanged = true;
}

void Transform::Scale(DirectX::XMFLOAT3 scale)
{
	this->scale.x *= scale.x;
	this->scale.y *= scale.y;
	this->scale.z *= scale.z;
	matrixChanged = true;
}

void Transform::UpdateMatrices()
{
	if (!matrixChanged) return;

	XMMATRIX pos = XMMatrixTranslationFromVector(XMLoadFloat3(&position));
	XMMATRIX rot = XMMatrixRotationRollPitchYawFromVector(XMLoadFloat3(&pitchYawRoll));
	XMMATRIX sc = XMMatrixScalingFromVector(XMLoadFloat3(&scale));

	// Note: Overloaded operators are defined in the DirectX namespace!
	// Alternatively, you can call XMMatrixMultiply(XMMatrixMultiply(s, r), t))
	XMMATRIX wMatrix = pos * rot * sc;

	XMStoreFloat4x4(&worldMatrix, wMatrix);
	XMStoreFloat4x4(&worldInverseTransposeMatrix,
		XMMatrixInverse(0, XMMatrixTranspose(wMatrix)));

	matrixChanged = false;
}

void Transform::UpdateVectors()
{
	if (!vectorChanged) return;

	XMVECTOR rot = XMQuaternionRotationRollPitchYawFromVector(XMLoadFloat3(&pitchYawRoll));
	XMStoreFloat3(&right, XMVector3Rotate(XMVectorSet(1, 0, 0, 0), rot));
	XMStoreFloat3(&up, XMVector3Rotate(XMVectorSet(0, 1, 0, 0), rot));
	XMStoreFloat3(&forward, XMVector3Rotate(XMVectorSet(0, 0, 1, 0), rot));

	vectorChanged = false;
}

Transform::Transform() :
	position(0, 0, 0),
	pitchYawRoll(0, 0, 0),
	scale(1, 1, 1),
	up(0, 1, 0),
	right(1, 0, 0),
	forward(0, 0, 1),
	matrixChanged(false),
	vectorChanged(false)
{
	XMStoreFloat4x4(&worldMatrix, XMMatrixIdentity());
	XMStoreFloat4x4(&worldInverseTransposeMatrix, XMMatrixIdentity());
}
