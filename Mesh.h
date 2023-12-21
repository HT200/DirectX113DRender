#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include "Vertex.h"
#include <string>

class Mesh {
private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> vb;
	Microsoft::WRL::ComPtr<ID3D11Buffer> ib;
	unsigned int iCount;
	void CreateBuffers(Vertex* vArray, int vCount, unsigned int* iArray, int iCount, Microsoft::WRL::ComPtr<ID3D11Device> device);
	void CalculateTangents(Vertex* verts, int numVerts, unsigned int* indices, int numIndices);

public:
	Microsoft::WRL::ComPtr<ID3D11Buffer> GetVertexBuffer();
	Microsoft::WRL::ComPtr<ID3D11Buffer> GetIndexBuffer();
	unsigned int GetIndexCount();

	void Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);

	Mesh(Vertex* vArray, int vCount, unsigned int* iArray, int iCount, Microsoft::WRL::ComPtr<ID3D11Device> device);
	Mesh(const std::wstring& objFile, Microsoft::WRL::ComPtr<ID3D11Device> device);
	~Mesh();
};