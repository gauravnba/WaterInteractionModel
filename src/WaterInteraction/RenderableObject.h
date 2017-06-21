#pragma once

#include <DXUT.h>
#include "DirectXMath.h"
#include <SDKmesh.h>
#include <string>

// Structures
//--------------------------------------------------------------------------------------
struct CBNeverChanges
{
	DirectX::XMFLOAT4 vLightDir;
};

struct CBChangesEveryFrame
{
	DirectX::XMFLOAT4X4 mWorldViewProj;
	DirectX::XMFLOAT4X4 mWorld;
};

// Forward declaration of C function to be used.
HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut);

namespace CustomDrawing
{
	class RenderableObject
	{
	public:
		RenderableObject();

		void* operator new(size_t obj);

		void operator delete(void* obj);

		virtual ~RenderableObject();

		RenderableObject(const RenderableObject& obj) = default;

		RenderableObject(RenderableObject&& obj) = default;

		HRESULT Initialize(std::wstring textureName, ID3D11Device* DXDevice);

		void Update(float deltaSeconds);

		void Draw(float deltaSeconds);

	private:
		float						mTranslateX;
		float						mTranslateY;
		float						mTranslateZ;
		ID3D11VertexShader*			mVertexShader;
		ID3D11PixelShader*			mPixelShader;
		ID3D11InputLayout*			mVertexLayout;
		CDXUTSDKMesh				mMesh;
		ID3D11Buffer*				mCBNeverChanges;
		ID3D11Buffer*				mCBChangesEveryFrame;
		ID3D11SamplerState*			mSamplerLinear;
		DirectX::XMMATRIX			mWorldMatrix;
		DirectX::XMMATRIX			mViewMatrix;
		DirectX::XMMATRIX			mProjectionMatrix;
	};
}