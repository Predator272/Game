#if defined(DEBUG) || defined(_DEBUG)
#define __CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#define DBG_NEW new (_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DBG_NEW
#endif

#include "helper.h"
#include "resource/resource.h"



int WINAPI _tWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPTSTR lpCmdLine, _In_ int nShowCmd)
{

#if defined(DEBUG) || defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	SetUnhandledExceptionFilter(VectoredExceptionHandler);

	try
	{
		std::shared_ptr<Engine::IApplication> App = Engine::IApplication::Create(_T("Game"), 50, 50, 640, 480);

		struct CAMERA_STRUCT
		{
			DirectX::XMVECTOR Eye;
			DirectX::XMVECTOR At;
			DirectX::XMVECTOR Up;
		} Camera = {
			DirectX::XMVectorSet(0.0f, 0.0f, -3.0f, 0.0f),
			DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
			DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
		};

		struct CONSTANT_BUFFER_STRUCT
		{
			DirectX::XMMATRIX World;
			DirectX::XMMATRIX View;
			DirectX::XMMATRIX Projection;
		} ConstantBufferStruct;

		POINT Size = {};
		DirectX::XMFLOAT2 Mouse = {};

		std::shared_ptr<Engine::IVertexShader> VertexShader;
		std::shared_ptr<Engine::IPixelShader> PixelShader;
		std::shared_ptr<Engine::IConstantBuffer> ConstantBuffer;

		std::shared_ptr<Engine::IVertexBuffer> VertexBuffer;
		std::shared_ptr<Engine::IIndexBuffer> IndexBuffer;
		std::shared_ptr<Engine::ITexture> Texture;

		{
			HRSRC hRsrc = FindResource(nullptr, MAKEINTRESOURCE(IDR_VERTEX_SHADER), RT_RCDATA);
			HGLOBAL hResData = LoadResource(nullptr, hRsrc);

			VertexShader = Engine::IVertexShader::Create(App, LockResource(hResData), SizeofResource(nullptr, hRsrc));

			UnlockResource(hResData);
			FreeResource(hResData);
		}

		{
			HRSRC hRsrc = FindResource(nullptr, MAKEINTRESOURCE(IDR_PIXEL_SHADER), RT_RCDATA);
			HGLOBAL hResData = LoadResource(nullptr, hRsrc);

			PixelShader = Engine::IPixelShader::Create(App, LockResource(hResData), SizeofResource(nullptr, hRsrc));

			UnlockResource(hResData);
			FreeResource(hResData);
		}

		ConstantBuffer = Engine::IConstantBuffer::Create(App, sizeof(ConstantBufferStruct));

		VertexShader->Set();
		PixelShader->Set();
		ConstantBuffer->SetVertexShader();

		{
			ResourceLoader::Model Model;
			if (!Model.Load("cube.obj"))
			{
				Assert(E_FAIL);
			}

			VertexBuffer = Engine::IVertexBuffer::Create(App, Model.GetVertex(), Model.GetVertexCount(), sizeof(ResourceLoader::VERTEX_STRUCT));
			IndexBuffer = Engine::IIndexBuffer::Create(App, Model.GetIndex(), Model.GetIndexCount());
		}

		{
			Microsoft::WRL::ComPtr<ID3DBlob> ImageSource;
			Assert(D3DReadFileToBlob(_T("cube.tga"), ImageSource.GetAddressOf()));

			ResourceLoader::Image Image;
			if (!Image.Load(ImageSource->GetBufferPointer(), ImageSource->GetBufferSize()))
			{
				Assert(E_FAIL);
			}

			Texture = Engine::ITexture::Create(App, Image.GetColor(), Image.GetWidth(), Image.GetHeight());
		}

		VertexBuffer->Set();
		IndexBuffer->Set();
		Texture->Set();

		while (App->Run())
		{
			if (App->GetKeyState(VK_ESCAPE))
			{
				App->Quit();
			}

			if (App->GetWidth() != Size.x || App->GetHeight() != Size.y)
			{
				Size.x = App->GetWidth();
				Size.y = App->GetHeight();

				App->ResizeBuffer(0, Size.x, Size.y, 4);
			}

			App->BeginDraw(0.5f, 0.5f, 0.5f, 1.0f);

			const float Time = Engine::GetTimer();

			static float LastTime = Time;
			const float Delta = Time - LastTime;
			LastTime = Time;

			const float Speed = 3.0f * static_cast<float>(Delta);
			DirectX::XMVECTOR SpeedVector = DirectX::XMVectorSet(Speed, Speed, Speed, Speed);

			Mouse.x += static_cast<float>(App->GetMouseDirectionX()) * 0.003f;
			Mouse.y += static_cast<float>(App->GetMouseDirectionY()) * 0.003f;
			constexpr float MinRadian = DirectX::XMConvertToRadians(-80.0f);
			constexpr float MaxRadian = DirectX::XMConvertToRadians(80.0f);
			Mouse.y = min(max(Mouse.y, MinRadian), MaxRadian);

			Camera.At = DirectX::XMVector3Transform(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), DirectX::XMMatrixRotationRollPitchYaw(Mouse.y, Mouse.x, 0.0f));

			DirectX::XMVECTOR Forward = DirectX::XMVector3Transform(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), DirectX::XMMatrixRotationRollPitchYaw(0.0f, Mouse.x, 0.0f));
			DirectX::XMVECTOR Left = DirectX::XMVector3Transform(DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), DirectX::XMMatrixRotationRollPitchYaw(0.0f, Mouse.x, 0.0f));

			if (App->GetKeyState('W')) Camera.Eye = DirectX::XMVectorAdd(Camera.Eye, DirectX::XMVectorMultiply(SpeedVector, Forward));
			if (App->GetKeyState('S')) Camera.Eye = DirectX::XMVectorSubtract(Camera.Eye, DirectX::XMVectorMultiply(SpeedVector, Forward));
			if (App->GetKeyState('D')) Camera.Eye = DirectX::XMVectorAdd(Camera.Eye, DirectX::XMVectorMultiply(SpeedVector, Left));
			if (App->GetKeyState('A')) Camera.Eye = DirectX::XMVectorSubtract(Camera.Eye, DirectX::XMVectorMultiply(SpeedVector, Left));
			if (App->GetKeyState('E')) Camera.Eye = DirectX::XMVectorAdd(Camera.Eye, DirectX::XMVectorMultiply(SpeedVector, Camera.Up));
			if (App->GetKeyState('Q')) Camera.Eye = DirectX::XMVectorSubtract(Camera.Eye, DirectX::XMVectorMultiply(SpeedVector, Camera.Up));

			ConstantBufferStruct.World = DirectX::XMMatrixTranspose(DirectX::XMMatrixIdentity());
			ConstantBufferStruct.View = DirectX::XMMatrixTranspose(DirectX::XMMatrixLookAtLH(Camera.Eye, DirectX::XMVectorAdd(Camera.Eye, Camera.At), Camera.Up));
			ConstantBufferStruct.Projection = DirectX::XMMatrixTranspose(DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV2, App->GetWidth() / static_cast<float>(App->GetHeight()), 0.01f, 1000.0f));

			ConstantBuffer->Update(&ConstantBufferStruct);
			App->Draw(IndexBuffer->GetIndexCount());

			App->EndDraw(0);
		}
	}
	catch (DWORD ErrorCode)
	{
		RaiseException(ErrorCode, 0, 0, nullptr);
	}

	return 0;
}
