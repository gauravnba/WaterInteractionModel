#include "pch.h"
#include "RenderableObject.h"
#include <SDKmisc.h>
#include "DirectXColors.h"

using namespace DirectX;

namespace CustomDrawing
{
	RenderableObject::RenderableObject() : 
		mTranslateX(0.0f), mTranslateY(0.0f), mTranslateZ(0.0f), 
		mVertexShader(nullptr), mPixelShader(nullptr), mVertexLayout(nullptr), mCBNeverChanges(nullptr), mCBChangesEveryFrame(nullptr), mSamplerLinear(nullptr)
	{
	}

	void* RenderableObject::operator new(size_t obj)
	{
		return _mm_malloc(obj, 16);
	}

	void RenderableObject::operator delete(void* obj)
	{
		return _mm_free(obj);
	}

	RenderableObject::~RenderableObject()
	{
		mMesh.Destroy();

		SAFE_RELEASE(mVertexLayout);
		SAFE_RELEASE(mVertexShader);
		SAFE_RELEASE(mPixelShader);
		SAFE_RELEASE(mCBNeverChanges);
		SAFE_RELEASE(mCBChangesEveryFrame);
		SAFE_RELEASE(mSamplerLinear);
	}

	HRESULT RenderableObject::Initialize(std::wstring fileName, ID3D11Device* d3dDevice)
	{
		HRESULT hr = S_OK;

		auto d3dImmediateContext = DXUTGetD3D11DeviceContext();

		DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
		// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
		// Setting this flag improves the shader debugging experience, but still allows 
		// the shaders to be optimized and to run exactly the way they will run in 
		// the release configuration of this program.
		dwShaderFlags |= D3DCOMPILE_DEBUG;

		// Disable optimizations to further improve shader debugging
		dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

		// Compile the vertex shader
		ID3DBlob* vsBlob = nullptr;
		V_RETURN(CompileShaderFromFile(L"render_object_vs_ps.hlsl", "VS", "vs_4_0", &vsBlob));

		// Create the vertex shader
		hr = d3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &mVertexShader);
		if (FAILED(hr))
		{
			SAFE_RELEASE(vsBlob);
			return hr;
		}

		// Define the input layout
		D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		UINT numElements = ARRAYSIZE(layout);

		// Create the input layout
		hr = d3dDevice->CreateInputLayout(layout, numElements, vsBlob->GetBufferPointer(),
			vsBlob->GetBufferSize(), &mVertexLayout);
		SAFE_RELEASE(vsBlob);
		if (FAILED(hr))
			return hr;

		// Set the input layout
		d3dImmediateContext->IASetInputLayout(mVertexLayout);

		// Compile the pixel shader
		ID3DBlob* psBlob = nullptr;
		V_RETURN(CompileShaderFromFile(L"render_object_vs_ps.hlsl", "PS", "ps_4_0", &psBlob));

		// Create the pixel shader
		hr = d3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &mPixelShader);
		SAFE_RELEASE(psBlob);
		if (FAILED(hr))
			return hr;

		// Load the mesh
		V_RETURN(mMesh.Create(d3dDevice, fileName.c_str()));

		// Create the constant buffers
		D3D11_BUFFER_DESC bd;
		ZeroMemory(&bd, sizeof(bd));
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(mCBChangesEveryFrame);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		V_RETURN(d3dDevice->CreateBuffer(&bd, nullptr, &mCBChangesEveryFrame));

		bd.ByteWidth = sizeof(mCBNeverChanges);
		V_RETURN(d3dDevice->CreateBuffer(&bd, nullptr, &mCBNeverChanges));

		//D3D11_MAPPED_SUBRESOURCE MappedResource;
		//V(d3dImmediateContext->Map(mCBNeverChanges, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		//auto pCB = reinterpret_cast<CBNeverChanges*>(MappedResource.pData);
		////XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(&pCB->vLightDir), s_LightDir);
		////pCB->vLightDir.w = 1.f;
		//d3dImmediateContext->Unmap(mCBNeverChanges, 0);

		// Initialize the world matrices
		mWorldMatrix = XMMatrixIdentity();

		// Initialize the view matrix
		static const XMVECTORF32 eye = { 0.0f, 3.0f, -500.0f, 0.f };
		static const XMVECTORF32 at = { 0.0f, 1.0f, 0.0f, 0.f };
		static const XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.f };
		mViewMatrix = DirectX::XMMatrixLookAtLH(eye, at, up);

		// Create the sample state
		D3D11_SAMPLER_DESC sampDesc;
		ZeroMemory(&sampDesc, sizeof(sampDesc));
		sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		sampDesc.MinLOD = 0;
		sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
		V_RETURN(d3dDevice->CreateSamplerState(&sampDesc, &mSamplerLinear));

		return S_OK;
	}

	void RenderableObject::Update(float deltaSeconds)
	{
		mWorldMatrix *= XMMatrixTranslation(mTranslateX, mTranslateY, mTranslateZ);
	}

	void RenderableObject::Draw(float deltaSeconds)
	{
		//
		// Clear the back buffer
		//
		auto pRTV = DXUTGetD3D11RenderTargetView();
		auto d3dImmediateContext = DXUTGetD3D11DeviceContext();
		d3dImmediateContext->ClearRenderTargetView(pRTV, Colors::MidnightBlue);

		//
		// Clear the depth stencil
		//
		auto pDSV = DXUTGetD3D11DepthStencilView();
		d3dImmediateContext->ClearDepthStencilView(pDSV, D3D11_CLEAR_DEPTH, 1.0, 0);

		XMMATRIX mWorldViewProjection = mWorldMatrix * mViewMatrix * mProjectionMatrix;

		// Update constant buffer that changes once per frame
		HRESULT hr;
		D3D11_MAPPED_SUBRESOURCE MappedResource;
		V(d3dImmediateContext->Map(mCBChangesEveryFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		auto pCB = reinterpret_cast<CBChangesEveryFrame*>(MappedResource.pData);
		XMStoreFloat4x4(&pCB->mWorldViewProj, XMMatrixTranspose(mWorldViewProjection));
		XMStoreFloat4x4(&pCB->mWorld, XMMatrixTranspose(mWorldMatrix));
		d3dImmediateContext->Unmap(mCBChangesEveryFrame, 0);

		//
		// Set the Vertex Layout
		//
		d3dImmediateContext->IASetInputLayout(mVertexLayout);

		//
		// Render the mesh
		//
		UINT Strides[1];
		UINT Offsets[1];
		ID3D11Buffer* pVB[1];
		pVB[0] = mMesh.GetVB11(0, 0);
		Strides[0] = static_cast<UINT>(mMesh.GetVertexStride(0, 0));
		Offsets[0] = 0;
		d3dImmediateContext->IASetVertexBuffers(0, 1, pVB, Strides, Offsets);
		d3dImmediateContext->IASetIndexBuffer(mMesh.GetIB11(0), mMesh.GetIBFormat11(0), 0);

		d3dImmediateContext->VSSetShader(mVertexShader, nullptr, 0);
		d3dImmediateContext->VSSetConstantBuffers(0, 1, &mCBNeverChanges);
		d3dImmediateContext->VSSetConstantBuffers(1, 1, &mCBChangesEveryFrame);

		d3dImmediateContext->PSSetShader(mPixelShader, nullptr, 0);
		d3dImmediateContext->PSSetConstantBuffers(1, 1, &mCBChangesEveryFrame);
		d3dImmediateContext->PSSetSamplers(0, 1, &mSamplerLinear);

		for (UINT subset = 0; subset < mMesh.GetNumSubsets(0); ++subset)
		{
			auto pSubset = mMesh.GetSubset(0, subset);

			auto PrimType = mMesh.GetPrimitiveType11((SDKMESH_PRIMITIVE_TYPE)pSubset->PrimitiveType);
			d3dImmediateContext->IASetPrimitiveTopology(PrimType);

			// Ignores most of the material information in them mesh to use only a simple shader
			auto pDiffuseRV = mMesh.GetMaterial(pSubset->MaterialID)->pDiffuseRV11;
			d3dImmediateContext->PSSetShaderResources(0, 1, &pDiffuseRV);

			d3dImmediateContext->DrawIndexed(static_cast<UINT>(pSubset->IndexCount), 0, static_cast<UINT>(pSubset->VertexStart));
		}
	}
}