#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "PathHelpers.h"
#include "SimpleShader.h"
#include "WICTextureLoader.h"
#include <memory>

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

// Needed for a helper function to load pre-compiled shader files
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// For the DirectX Math library
using namespace DirectX;

// --------------------------------------------------------
// Constructor
//
// DXCore (base class) constructor will set up underlying fields.
// Direct3D itself, and our window, are not ready at this point!
//
// hInstance - the application's OS-level handle (unique ID)
// --------------------------------------------------------
Game::Game(HINSTANCE hInstance)
	: DXCore(
		hInstance,			// The application's handle
		L"DirectX Game",	// Text for the window's title bar (as a wide-character string)
		1280,				// Width of the window's client area
		720,				// Height of the window's client area
		false,				// Sync the framerate to the monitor refresh? (lock framerate)
		true),				// Show extra stats (fps) in title bar?
	cam(true),
	ambientColor(0.0f, 0.1f, 0.25f),
	shadowMapResolution(1024),
	shadowProjectionSize(10.0f),
	shadowViewMatrix(),
	shadowProjectionMatrix(),
	blurriness(0)
{
#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.\n");
#endif
}

// --------------------------------------------------------
// Destructor - Clean up anything our game has created:
//  - Delete all objects manually created within this class
//  - Release() all Direct3D objects created within this class
//  - Release() all Direct3D objects created within this class
// --------------------------------------------------------
Game::~Game()
{
	// Call delete or delete[] on any objects or arrays you've
	// created using new or new[] within this class
	// - Note: this is unnecessary if using smart pointers

	// Call Release() on any Direct3D objects made within this class
	// - Note: this is unnecessary for D3D objects stored in ComPtrs

	// ImGui clean up
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

// --------------------------------------------------------
// Called once per program, after Direct3D and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	// Initialize ImGui itself & platform/renderer backends
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(device.Get(), context.Get());
	ImGui::StyleColorsDark();

	LoadShaders();
	CreateGeometry();
	CreateLight();
	CreateShadowMap();
	
	// Set initial graphics API state
	//  - These settings persist until we change them
	//  - Some of these, like the primitive topology & input layout, probably won't change
	//  - Others, like setting shaders, will need to be moved elsewhere later
	{
		// Tell the input assembler (IA) stage of the pipeline what kind of
		// geometric primitives (points, lines or triangles) we want to draw.  
		// Essentially: "What kind of shape should the GPU draw with our vertices?"
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// Make camera
	camera = std::make_shared<Camera>(
		0.0f, 1.5f, -15.0f,					// Position
		(float)windowWidth / windowHeight,	// Aspect ratio
		XM_PIDIV4,							// FoV
		3.0f,								// Movement speed
		0.001f,								// Mouse looking speed
		0.01f,								// Near clip
		100.0f								// Far clip
	);

	camera2 = std::make_shared<Camera>(
		2.0f, 2.0f, -3.0f,					// Position
		(float)windowWidth / windowHeight,	// Aspect ratio
		XM_PIDIV2,							// FoV
		3.0f,								// Movement speed
		0.001f,								// Mouse looking speed
		0.01f,								// Near clip
		100.0f								// Far clip
	);
}

// --------------------------------------------------------
// Loads shaders from compiled shader object (.cso) files
// and also created the Input Layout that describes our 
// vertex data to the rendering pipeline. 
// - Input Layout creation is done here because it must 
//    be verified against vertex shader byte code
// - We'll have that byte code already loaded below
// --------------------------------------------------------
void Game::LoadShaders()
{
	vertexShader = std::make_shared<SimpleVertexShader>(device, context, FixPath(L"VertexShader.cso").c_str());
	pixelShader = std::make_shared<SimplePixelShader>(device, context, FixPath(L"PixelShader.cso").c_str());
	customPS = std::make_shared<SimplePixelShader>(device, context, FixPath(L"CustomPS.cso").c_str());
	skyVS = std::make_shared<SimpleVertexShader>(device, context, FixPath(L"SkyVS.cso").c_str());
	skyPS = std::make_shared<SimplePixelShader>(device, context, FixPath(L"SkyPS.cso").c_str());
	ppVS = std::make_shared<SimpleVertexShader>(device, context, FixPath(L"FullscreenVS.cso").c_str());
	ppPS = std::make_shared<SimplePixelShader>(device, context, FixPath(L"PostProcessPS.cso").c_str());
}

// --------------------------------------------------------
// Creates the geometry we're going to draw
// --------------------------------------------------------
void Game::CreateGeometry()
{
	// Create a sampler state
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
	D3D11_SAMPLER_DESC sampDesc = {};

	ResizePostProcess();

	// Sampler state for texture
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;		// Defines how to handle
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;		// addresses outside the 
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;		// 0 – 1 UV range

	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;			// How to handle sampling “between” pixels
	sampDesc.MaxAnisotropy = 16;

	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;				// Enable mipmapping at any range
	device->CreateSamplerState(&sampDesc, sampler.GetAddressOf());

	// Sampler state for post processing
	D3D11_SAMPLER_DESC ppSampDesc = {};
	ppSampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	ppSampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	ppSampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	ppSampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	ppSampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	device->CreateSamplerState(&ppSampDesc, ppSampler.GetAddressOf());

	// Load texture
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cobbleA, cobbleN, cobbleR, cobbleM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> floorA, floorN, floorR, floorM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> paintA, paintN, paintR, paintM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> scratchedA, scratchedN, scratchedR, scratchedM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bronzeA, bronzeN, bronzeR, bronzeM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> roughA, roughN, roughR, roughM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> woodA, woodN, woodR, woodM;

// From demo - Quick pre-processor macro for simplifying texture loading calls below
#define LoadTexture(path, srv) CreateWICTextureFromFile(device.Get(), context.Get(), FixPath(path).c_str(), 0, srv.GetAddressOf());

	LoadTexture(L"../../Assets/Textures/cobblestone_albedo.png", cobbleA);
	LoadTexture(L"../../Assets/Textures/cobblestone_normals.png", cobbleN);
	LoadTexture(L"../../Assets/Textures/cobblestone_roughness.png", cobbleR);
	LoadTexture(L"../../Assets/Textures/cobblestone_metal.png", cobbleM);

	LoadTexture(L"../../Assets/Textures/floor_albedo.png", floorA);
	LoadTexture(L"../../Assets/Textures/floor_normals.png", floorN);
	LoadTexture(L"../../Assets/Textures/floor_roughness.png", floorR);
	LoadTexture(L"../../Assets/Textures/floor_metal.png", floorM);

	LoadTexture(L"../../Assets/Textures/paint_albedo.png", paintA);
	LoadTexture(L"../../Assets/Textures/paint_normals.png", paintN);
	LoadTexture(L"../../Assets/Textures/paint_roughness.png", paintR);
	LoadTexture(L"../../Assets/Textures/paint_metal.png", paintM);

	LoadTexture(L"../../Assets/Textures/scratched_albedo.png", scratchedA);
	LoadTexture(L"../../Assets/Textures/scratched_normals.png", scratchedN);
	LoadTexture(L"../../Assets/Textures/scratched_roughness.png", scratchedR);
	LoadTexture(L"../../Assets/Textures/scratched_metal.png", scratchedM);

	LoadTexture(L"../../Assets/Textures/bronze_albedo.png", bronzeA);
	LoadTexture(L"../../Assets/Textures/bronze_normals.png", bronzeN);
	LoadTexture(L"../../Assets/Textures/bronze_roughness.png", bronzeR);
	LoadTexture(L"../../Assets/Textures/bronze_metal.png", bronzeM);

	LoadTexture(L"../../Assets/Textures/rough_albedo.png", roughA);
	LoadTexture(L"../../Assets/Textures/rough_normals.png", roughN);
	LoadTexture(L"../../Assets/Textures/rough_roughness.png", roughR);
	LoadTexture(L"../../Assets/Textures/rough_metal.png", roughM);

	LoadTexture(L"../../Assets/Textures/wood_albedo.png", woodA);
	LoadTexture(L"../../Assets/Textures/wood_normals.png", woodN);
	LoadTexture(L"../../Assets/Textures/wood_roughness.png", woodR);
	LoadTexture(L"../../Assets/Textures/wood_metal.png", woodM);
	
	// Load meshes
	std::shared_ptr<Mesh> cubeMesh = std::make_shared<Mesh>(FixPath(L"../../Assets/Models/cube.obj").c_str(), device);
	std::shared_ptr<Mesh> cylinderMesh = std::make_shared<Mesh>(FixPath(L"../../Assets/Models/cylinder.obj").c_str(), device);
	std::shared_ptr<Mesh> helixMesh = std::make_shared<Mesh>(FixPath(L"../../Assets/Models/helix.obj").c_str(), device);
	std::shared_ptr<Mesh> sphereMesh = std::make_shared<Mesh>(FixPath(L"../../Assets/Models/sphere.obj").c_str(), device);
	std::shared_ptr<Mesh> torusMesh = std::make_shared<Mesh>(FixPath(L"../../Assets/Models/torus.obj").c_str(), device);
	std::shared_ptr<Mesh> quadMesh = std::make_shared<Mesh>(FixPath(L"../../Assets/Models/quad.obj").c_str(), device);
	std::shared_ptr<Mesh> quadDSMesh = std::make_shared<Mesh>(FixPath(L"../../Assets/Models/quad_double_sided.obj").c_str(), device);
	meshes.insert(meshes.end(), { cubeMesh, cylinderMesh, helixMesh, sphereMesh, torusMesh, quadMesh, quadDSMesh });

	// Create materials
	// Cobblestone
	std::shared_ptr<Material> cobbleMat = std::make_shared<Material>(pixelShader, vertexShader);
	cobbleMat->AddSampler("BasicSampler", sampler);
	cobbleMat->AddTextureSRV("Albedo", cobbleA);
	cobbleMat->AddTextureSRV("NormalMap", cobbleN);
	cobbleMat->AddTextureSRV("RoughnessMap", cobbleR);
	cobbleMat->AddTextureSRV("MetalnessMap", cobbleM);

	// Floor
	std::shared_ptr<Material> floorMat = std::make_shared<Material>(pixelShader, vertexShader);
	floorMat->AddSampler("BasicSampler", sampler);
	floorMat->AddTextureSRV("Albedo", floorA);
	floorMat->AddTextureSRV("NormalMap", floorN);
	floorMat->AddTextureSRV("RoughnessMap", floorR);
	floorMat->AddTextureSRV("MetalnessMap", floorM);

	// Paint
	std::shared_ptr<Material> paintMat = std::make_shared<Material>(pixelShader, vertexShader);
	paintMat->AddSampler("BasicSampler", sampler);
	paintMat->AddTextureSRV("Albedo", paintA);
	paintMat->AddTextureSRV("NormalMap", paintN);
	paintMat->AddTextureSRV("RoughnessMap", paintR);
	paintMat->AddTextureSRV("MetalnessMap", paintM);

	// Scratched metal
	std::shared_ptr<Material> scratchedMat = std::make_shared<Material>(pixelShader, vertexShader);
	scratchedMat->AddSampler("BasicSampler", sampler);
	scratchedMat->AddTextureSRV("Albedo", scratchedA);
	scratchedMat->AddTextureSRV("NormalMap", scratchedN);
	scratchedMat->AddTextureSRV("RoughnessMap", scratchedR);
	scratchedMat->AddTextureSRV("MetalnessMap", scratchedM);

	// Bronze
	std::shared_ptr<Material> bronzeMat = std::make_shared<Material>(pixelShader, vertexShader);
	bronzeMat->AddSampler("BasicSampler", sampler);
	bronzeMat->AddTextureSRV("Albedo", bronzeA);
	bronzeMat->AddTextureSRV("NormalMap", bronzeN);
	bronzeMat->AddTextureSRV("RoughnessMap", bronzeR);
	bronzeMat->AddTextureSRV("MetalnessMap", bronzeM);

	// Rough
	std::shared_ptr<Material> roughMat = std::make_shared<Material>(pixelShader, vertexShader);
	roughMat->AddSampler("BasicSampler", sampler);
	roughMat->AddTextureSRV("Albedo", roughA);
	roughMat->AddTextureSRV("NormalMap", roughN);
	roughMat->AddTextureSRV("RoughnessMap", roughR);
	roughMat->AddTextureSRV("MetalnessMap", roughM);

	// Wood
	std::shared_ptr<Material> woodMat = std::make_shared<Material>(pixelShader, vertexShader);
	woodMat->AddSampler("BasicSampler", sampler);
	woodMat->AddTextureSRV("Albedo", woodA);
	woodMat->AddTextureSRV("NormalMap", woodN);
	woodMat->AddTextureSRV("RoughnessMap", woodR);
	woodMat->AddTextureSRV("MetalnessMap", woodM);

	materials.insert(materials.end(), { cobbleMat, floorMat, paintMat, scratchedMat, bronzeMat, roughMat, woodMat });

	// Create entities
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, cobbleMat));
	entities.push_back(std::make_shared<GameEntity>(cylinderMesh, floorMat));
	entities.push_back(std::make_shared<GameEntity>(helixMesh, paintMat));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, scratchedMat));
	entities.push_back(std::make_shared<GameEntity>(torusMesh, bronzeMat));
	entities.push_back(std::make_shared<GameEntity>(quadMesh, roughMat));
	entities.push_back(std::make_shared<GameEntity>(quadDSMesh, woodMat));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, woodMat));

	entities[0]->GetTransform()->MoveAbsolute(-9, 0, 0);
	entities[1]->GetTransform()->MoveAbsolute(-6, 0, 0);
	entities[2]->GetTransform()->MoveAbsolute(-3, 0, 0);
	entities[3]->GetTransform()->MoveAbsolute(0, 0, 0);
	entities[4]->GetTransform()->MoveAbsolute(3, 0, 0);
	entities[5]->GetTransform()->MoveAbsolute(6, -1, 0);
	entities[6]->GetTransform()->MoveAbsolute(9, -1, 0);
	entities[7]->GetTransform()->SetScale(20, 20, 20); 
	entities[7]->GetTransform()->MoveAbsolute(0, -1.25, 0);

	// Create the sky
	sky = std::make_shared<Sky>(
		FixPath(L"../../Assets/Skies/right.png").c_str(),
		FixPath(L"../../Assets/Skies/left.png").c_str(),
		FixPath(L"../../Assets/Skies/up.png").c_str(),
		FixPath(L"../../Assets/Skies/down.png").c_str(),
		FixPath(L"../../Assets/Skies/front.png").c_str(),
		FixPath(L"../../Assets/Skies/back.png").c_str(),
		cubeMesh,
		skyVS,
		skyPS,
		sampler,
		device,
		context);
}

void Game::ResizePostProcess()
{
	// Describe the texture we're creating
	D3D11_TEXTURE2D_DESC textureDesc = {};
	textureDesc.Width = windowWidth;
	textureDesc.Height = windowHeight;
	textureDesc.ArraySize = 1;
	textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.MipLevels = 1;
	textureDesc.MiscFlags = 0;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;

	// Create the resource (no need to track it after the views are created below)
	Microsoft::WRL::ComPtr<ID3D11Texture2D> ppTexture;
	device->CreateTexture2D(&textureDesc, 0, ppTexture.GetAddressOf());

	// Create the Render Target View
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = textureDesc.Format;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	device->CreateRenderTargetView(
		ppTexture.Get(),
		&rtvDesc,
		ppRTV.ReleaseAndGetAddressOf());

	// Create the Shader Resource View
	// By passing it a null description for the SRV, we
	// get a "default" SRV that has access to the entire resource
	device->CreateShaderResourceView(
		ppTexture.Get(),
		0,
		ppSRV.ReleaseAndGetAddressOf());
}

// Create all types of light in the scene
void Game::CreateLight()
{
	Light light1 = {};
	light1.Color = XMFLOAT3(1, 1, 1);
	light1.Type = LIGHT_TYPE_DIRECTIONAL;
	light1.Intensity = 1.0f;
	light1.Direction = XMFLOAT3(1, 0, 0.6);

	Light light2 = {};
	light2.Color = XMFLOAT3(1, 1, 1);
	light2.Type = LIGHT_TYPE_DIRECTIONAL;
	light2.Intensity = 1.0f;
	light2.Direction = XMFLOAT3(0, -1, 0);

	Light light3 = {};
	light3.Color = XMFLOAT3(1, 1, 1);
	light3.Type = LIGHT_TYPE_DIRECTIONAL;
	light3.Intensity = 1.0f;
	light3.Direction = XMFLOAT3(-1, 1, -0.5f);

	Light light4 = {};
	light4.Color = XMFLOAT3(1, 1, 1);
	light4.Type = LIGHT_TYPE_POINT;
	light4.Intensity = 1.0f;
	light4.Position = XMFLOAT3(-1.5f, 0, 0);
	light4.Range = 10.0f;

	Light light5 = {};
	light5.Color = XMFLOAT3(1, 1, 1);
	light5.Type = LIGHT_TYPE_POINT;
	light5.Intensity = 0.5f;
	light5.Position = XMFLOAT3(1.5f, 0, 0);
	light5.Range = 10.0f;

	lights.insert(lights.end(), { light1, light2, light3, light4, light5 });
}

void Game::CreateShadowMap()
{
	// Create the actual texture that will be the shadow map
	D3D11_TEXTURE2D_DESC shadowDesc = {};
	shadowDesc.Width = shadowMapResolution; // Ideally a power of 2 (like 1024)
	shadowDesc.Height = shadowMapResolution; // Ideally a power of 2 (like 1024)
	shadowDesc.ArraySize = 1;
	shadowDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	shadowDesc.CPUAccessFlags = 0;
	shadowDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	shadowDesc.MipLevels = 1;
	shadowDesc.MiscFlags = 0;
	shadowDesc.SampleDesc.Count = 1;
	shadowDesc.SampleDesc.Quality = 0;
	shadowDesc.Usage = D3D11_USAGE_DEFAULT;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> shadowTexture;
	device->CreateTexture2D(&shadowDesc, 0, shadowTexture.GetAddressOf());

	// Create the depth/stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC shadowDSDesc = {};
	shadowDSDesc.Format = DXGI_FORMAT_D32_FLOAT;
	shadowDSDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	shadowDSDesc.Texture2D.MipSlice = 0;
	device->CreateDepthStencilView(
		shadowTexture.Get(),
		&shadowDSDesc,
		shadowDSV.GetAddressOf());

	// Create the SRV for the shadow map
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	device->CreateShaderResourceView(
		shadowTexture.Get(),
		&srvDesc,
		shadowSRV.GetAddressOf());

	// Create the special "comparison" sampler state for shadows
	D3D11_SAMPLER_DESC shadowSampDesc = {};
	shadowSampDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; // COMPARISON filter!
	shadowSampDesc.ComparisonFunc = D3D11_COMPARISON_LESS;
	shadowSampDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	shadowSampDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	shadowSampDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	shadowSampDesc.BorderColor[0] = 1.0f;
	shadowSampDesc.BorderColor[1] = 1.0f;
	shadowSampDesc.BorderColor[2] = 1.0f;
	shadowSampDesc.BorderColor[3] = 1.0f;
	device->CreateSamplerState(&shadowSampDesc, &shadowSampler);

	// Create a rasterizer state
	D3D11_RASTERIZER_DESC shadowRastDesc = {};
	shadowRastDesc.FillMode = D3D11_FILL_SOLID;
	shadowRastDesc.CullMode = D3D11_CULL_BACK;
	shadowRastDesc.DepthClipEnable = true;
	shadowRastDesc.DepthBias = 1000; // Multiplied by (smallest possible positive value storable in the depth buffer)
	shadowRastDesc.DepthBiasClamp = 0.0f;
	shadowRastDesc.SlopeScaledDepthBias = 1.0f;
	device->CreateRasterizerState(&shadowRastDesc, &shadowRasterizer);

	// Create the "camera" matrices for the shadow map rendering

	// View
	XMMATRIX shView = XMMatrixLookAtLH(
		XMVectorSet(0, 20, -20, 0),
		XMVectorSet(0, 0, 0, 0),
		XMVectorSet(0, 1, 0, 0));
	XMStoreFloat4x4(&shadowViewMatrix, shView);

	// Projection - we want ORTHOGRAPHIC for directional light shadows
	// NOTE: This particular projection is set up to be SMALLER than
	// the overall "scene", to show what happens when objects go
	// outside the shadow area.  In a game, you'd never want the
	// user to see this edge, but I'm specifically making the projection
	// small in this demo to show you that it CAN happen.
	//
	// Ideally, the first two parameters below would be adjusted to
	// fit the scene (or however much of the scene the user can see
	// at a time).  More advanced techniques, like cascaded shadow maps,
	// would use multiple (usually 4) shadow maps with increasingly larger
	// projections to ensure large open world games have shadows "everywhere"
	XMMATRIX shProj = XMMatrixOrthographicLH(shadowProjectionSize, shadowProjectionSize, 0.1f, 100.0f);
	XMStoreFloat4x4(&shadowProjectionMatrix, shProj);

}

void Game::RenderShadowMap()
{
	// Initial pipeline setup - No RTV necessary - Clear shadow map
	context->OMSetRenderTargets(0, 0, shadowDSV.Get());
	context->ClearDepthStencilView(shadowDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	context->RSSetState(shadowRasterizer.Get());

	// Change viewport
	D3D11_VIEWPORT viewport = {};
	viewport.Width = (float)shadowMapResolution;
	viewport.Height = (float)shadowMapResolution;
	viewport.MaxDepth = 1.0f;
	context->RSSetViewports(1, &viewport);

	// Turn on ShadowVS
	std::shared_ptr<SimpleVertexShader> shadowVS = std::make_shared<SimpleVertexShader>(device, context, FixPath(L"ShadowVS.cso").c_str());
	shadowVS->SetShader();
	shadowVS->SetMatrix4x4("view", shadowViewMatrix);
	shadowVS->SetMatrix4x4("projection", shadowProjectionMatrix);
	context->PSSetShader(0, 0, 0);

	// Loop and draw all entities
	for (auto& e : entities)
	{
		shadowVS->SetMatrix4x4("world", e->GetTransform()->GetWorldMatrix());
		shadowVS->CopyAllBufferData();

		// Draw the mesh directly to avoid the entity's material
		// Note: Your code may differ significantly here!
		e->GetMesh()->Draw(context);
	}

	// Go back to the screen
	viewport.Width = (float)this->windowWidth;
	viewport.Height = (float)this->windowHeight;
	context->RSSetViewports(1, &viewport);
	context->OMSetRenderTargets(
		1,
		backBufferRTV.GetAddressOf(),
		depthBufferDSV.Get());
}

void Game::SetupUI()
{
	// General Details
	if (ImGui::TreeNode("General"))
	{
		ImGui::Text("Frame rate: %i fps", (int)ImGui::GetIO().Framerate);
		ImGui::Text("Window size: %i x %i", windowWidth, windowHeight);
		ImGui::TreePop();
	}

	// Entities
	if (ImGui::TreeNode("Entities"))
	{
		for (int i = 0; i < entities.size(); i++)
		{
			ImGui::PushID(i);
			if (ImGui::TreeNode("Entity", "Entity %i", i))
			{
				ImGui::Spacing();

				Transform* trans = entities[i]->GetTransform();
				XMFLOAT3 pos = trans->GetPosition();
				XMFLOAT3 rot = trans->GetPitchYawRoll();
				XMFLOAT3 sc = trans->GetScale();

				if (ImGui::DragFloat3("Position", &pos.x, 0.01f)) trans->SetPosition(pos);
				if (ImGui::DragFloat3("Rotation (Radians)", &rot.x, 0.01f)) trans->SetRotation(rot);
				if (ImGui::DragFloat3("Scale", &sc.x, 0.01f)) trans->SetScale(sc);

				ImGui::Spacing();

				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	// Camera
	if (ImGui::TreeNode("Camera"))
	{
		std::shared_ptr<Camera> temp = (cam) ? camera : camera2;
		XMFLOAT3 position = temp->GetTransform()->GetPosition();
		float fov = temp->GetFieldOfView();
		if (ImGui::Button("Change camera")) cam = !cam;
		ImGui::SameLine(); 
		ImGui::Text("Camera: %s", (cam) ? "Camera 1" : "Camera 2");
		ImGui::Text("FOV: %f", fov);
		ImGui::Text("X: %f", position.x);
		ImGui::Text("Y: %f", position.y);
		ImGui::Text("Z: %f", position.z);
		ImGui::TreePop();
	}

	// Lights
	if (ImGui::TreeNode("Lights"))
	{
		for (int i = 0; i < lights.size(); i++)
		{
			std::string lName = "Light %d";

			ImGui::PushID(i);
			if (ImGui::TreeNode("Light Node", lName.c_str(), i))
			{
				ImGui::DragFloat3("Direction", &lights[i].Direction.x, 0.1f);
				XMStoreFloat3(&lights[i].Direction, XMVector3Normalize(XMLoadFloat3(&lights[i].Direction)));
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	// Box Blur
	if (ImGui::TreeNode("Box Blur"))
	{
		ImGui::SliderInt("Blurriness", &blurriness, 0, 10);
		ImGui::TreePop();
	}
}


// --------------------------------------------------------
// Handle resizing to match the new window size.
//  - DXCore needs to resize the back buffer
//  - Eventually, we'll want to update our 3D camera
// --------------------------------------------------------
void Game::OnResize()
{
	// Handle base-level DX resize stuff
	DXCore::OnResize();

	// Update camera's projection matrix
	if (camera) camera->UpdateProjectionMatrix((float)windowWidth / windowHeight);
	if (camera2) camera2->UpdateProjectionMatrix((float)windowWidth / windowHeight);
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Feed fresh input data to ImGui
	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = deltaTime;
	io.DisplaySize.x = (float)this->windowWidth;
	io.DisplaySize.y = (float)this->windowHeight;

	// Reset the frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Determine new input capture
	Input& input = Input::GetInstance();
	input.SetKeyboardCapture(io.WantCaptureKeyboard);
	input.SetMouseCapture(io.WantCaptureMouse);

	SetupUI();

	// Example input checking: Quit if the escape key is pressed
	if (Input::GetInstance().KeyDown(VK_ESCAPE))
		Quit();

	//entities[4]->GetTransform()->Rotate(0, 0, deltaTime * 1.0f);

	if (cam) camera->Update(deltaTime);
	else camera2->Update(deltaTime);
}

void Game::PreRender()
{
	// Clear the back buffer (erases what's on the screen)
	const float bgColor[4] = { 0.4f, 0.6f, 0.75f, 1.0f }; // Cornflower Blue
	context->ClearRenderTargetView(backBufferRTV.Get(), bgColor);

	// Clear the depth buffer (resets per-pixel occlusion information)
	context->ClearDepthStencilView(depthBufferDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	// Clear render targets
	context->ClearRenderTargetView(ppRTV.Get(), bgColor);

	context->OMSetRenderTargets(1, ppRTV.GetAddressOf(), depthBufferDSV.Get());
}

void Game::PostRender()
{
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), 0);

	// Activate shaders and bind resources
	// Also set any required cbuffer data (not shown)
	ppVS->SetShader();
	ppPS->SetShader();

	ppPS->SetShaderResourceView("Pixels", ppSRV.Get());
	ppPS->SetSamplerState("ClampSampler", ppSampler.Get());
	ppPS->SetInt("blurRadius", blurriness);
	ppPS->SetFloat("pixelWidth", 1.0f / windowWidth);
	ppPS->SetFloat("pixelHeight", 1.0f / windowHeight);
	ppPS->CopyAllBufferData();

	context->Draw(3, 0); // Draw exactly 3 vertices (one triangle)
}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	RenderShadowMap();
	PreRender();

	// Draw all game entities
	for (auto& e : entities)
	{
		std::shared_ptr<SimpleVertexShader> vs = e->GetMaterial()->GetVertexShader();
		vs->SetMatrix4x4("lightView", shadowViewMatrix);
		vs->SetMatrix4x4("lightProjection", shadowProjectionMatrix);

		// Set data in shader's buffer
		std::shared_ptr<SimplePixelShader> ps = e->GetMaterial()->GetPixelShader();
		ps->SetFloat("time", deltaTime);
		ps->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());
		ps->SetShaderResourceView("ShadowMap", shadowSRV);
		ps->SetSamplerState("ShadowSampler", shadowSampler);

		// Draw an entity
		e->Draw(context, (cam) ? camera : camera2);
	}

	sky->Draw(cam ? camera : camera2);

	PostRender();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	ID3D11ShaderResourceView* nullSRVs[128] = {};
	context->PSSetShaderResources(0, 128, nullSRVs);

	// Frame END
	// - These should happen exactly ONCE PER FRAME
	// - At the very end of the frame (after drawing *everything*)
	{
		// Present the back buffer to the user
		//  - Puts the results of what we've drawn onto the window
		//  - Without this, the user never sees anything
		bool vsyncNecessary = vsync || !deviceSupportsTearing || isFullscreen;
		swapChain->Present(
			vsyncNecessary ? 1 : 0,
			vsyncNecessary ? 0 : DXGI_PRESENT_ALLOW_TEARING);

		// Must re-bind buffers after presenting, as they become unbound
		context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthBufferDSV.Get());
	}
}