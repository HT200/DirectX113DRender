#include "Material.h"

std::shared_ptr<SimplePixelShader> Material::GetPixelShader() { return pixelShader; }
std::shared_ptr<SimpleVertexShader> Material::GetVertexShader() { return vertexShader; }

void Material::SetPixelShader(std::shared_ptr<SimplePixelShader> ps) { pixelShader = ps; }
void Material::SetVertexShader(std::shared_ptr<SimpleVertexShader> vs) { vertexShader = vs; }

void Material::AddTextureSRV(std::string name, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv) { textureSRVs.insert({ name, srv }); }
void Material::AddSampler(std::string name, Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler) { samplers.insert({ name, sampler }); }

Material::Material(std::shared_ptr<SimplePixelShader> ps, std::shared_ptr<SimpleVertexShader> vs) :
	pixelShader(ps),
	vertexShader(vs)
{}

void Material::PrepareMaterial(Transform * transform, std::shared_ptr<Camera> camera)
{
	pixelShader->SetShader();
	vertexShader->SetShader();

	vertexShader->SetMatrix4x4("world", transform->GetWorldMatrix());
	vertexShader->SetMatrix4x4("view", camera->GetView());
	vertexShader->SetMatrix4x4("projection", camera->GetProjection());
	vertexShader->SetMatrix4x4("worldInvTrans", transform->GetWorldInverseTransposeMatrix());
	vertexShader->CopyAllBufferData();

	pixelShader->SetFloat3("cameraPosition", camera->GetTransform()->GetPosition());
	pixelShader->CopyAllBufferData();

	for (auto& t : textureSRVs) { pixelShader->SetShaderResourceView(t.first.c_str(), t.second); }
	for (auto& s : samplers) { pixelShader->SetSamplerState(s.first.c_str(), s.second); }
}
