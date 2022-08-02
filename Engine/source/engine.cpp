#include "include/engine.h"
#include <windows.h>
#include <windowsx.h>
#include <hidsdi.h>
#include <wrl/client.h>
#include <ctime>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace Engine
{
	float GetTimer()
	{
		return clock() / 1000.0f;
	}



	inline void Check(HRESULT ErrorCode)
	{
		if (FAILED(ErrorCode))
		{
			throw ErrorCode;
		}
	}

	template<class Class, typename... Param>
	inline std::shared_ptr<Class> CreateInterface(Param... Parameters)
	{
		return std::make_shared<Class>(Parameters...);
	}



	class Application : public IApplication
	{
	public:
		Application(LPCTSTR Title, INT X, INT Y, UINT Width, UINT Height, DWORD Style)
		{
			GetModuleFileName(this->hInstance, this->FileName, MAX_PATH);

			const HICON hIcon = ExtractIcon(this->hInstance, this->FileName, 0);

			const WNDCLASS WndClass = {
				0, StartWndProc, 0, 0,
				this->hInstance,
				(hIcon ? hIcon : LoadIcon(nullptr, IDI_APPLICATION)),
				LoadCursor(nullptr, IDC_ARROW),
				GetStockBrush(BLACK_BRUSH),
				nullptr, Title,
			};

			if (!RegisterClass(&WndClass))
			{
				throw GetLastError();
			}

			RECT Rect = { X, Y,  static_cast<INT>(Width), static_cast<INT>(Height) };

			if (!AdjustWindowRect(&Rect, Style, false))
			{
				throw GetLastError();
			}

			if (!(this->hWnd = CreateWindow(WndClass.lpszClassName, WndClass.lpszClassName, WS_VISIBLE | Style, Rect.left, Rect.top, Rect.right - Rect.left + X, Rect.bottom - Rect.top + Y, nullptr, nullptr, WndClass.hInstance, this)))
			{
				throw GetLastError();
			}

			this->CreateDevice(1, this->GetWidth(), this->GetHeight(), 1);
		}

		~Application()
		{
			this->DeviceContext->ClearState();
		}

		bool Run() override
		{
			ZeroMemory(&this->Mouse, sizeof(this->Mouse));

			while (this->Msg.message != WM_QUIT)
			{
				if (PeekMessage(&this->Msg, nullptr, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&this->Msg);
					DispatchMessage(&this->Msg);
				}
				else
				{
					return true;
				}
			}

			return false;
		}

		void Quit() override
		{
			DestroyWindow(this->hWnd);
		}

		int GetWidth() const override
		{
			return this->WndSize.x;
		}

		int GetHeight() const override
		{
			return this->WndSize.y;
		}

		int GetMouseDirectionX() const override
		{
			return this->Mouse.x;
		}

		int GetMouseDirectionY() const override
		{
			return this->Mouse.y;
		}

		bool GetKeyState(BYTE KeyCode) const override
		{
			return this->KeyState[KeyCode];
		}

		bool ResizeBuffer(UINT BufferCount, UINT Width, UINT Height, UINT SampleCount) override
		{
			try
			{
				this->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
				this->RenderTargetView.Reset();
				this->DepthStencilView.Reset();
				this->DeviceContext->Flush();

				DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
				Check(this->SwapChain->GetDesc(&SwapChainDesc));

				if (SampleCount == 0 || SampleCount == SwapChainDesc.SampleDesc.Count)
				{
					Check(this->SwapChain->ResizeBuffers(BufferCount, Width, Height, DXGI_FORMAT_UNKNOWN, 0));
				}
				else
				{
					if (BufferCount)
					{
						SwapChainDesc.BufferCount = BufferCount;
					}

					if (Width)
					{
						SwapChainDesc.BufferDesc.Width = Width;
					}

					if (Height)
					{
						SwapChainDesc.BufferDesc.Height = Height;
					}

					if (SampleCount)
					{
						SwapChainDesc.SampleDesc.Count = SampleCount;
					}

					Microsoft::WRL::ComPtr<IDXGIDevice> DXGIDevice;
					Microsoft::WRL::ComPtr<IDXGIAdapter> DXGIAdapter;
					Microsoft::WRL::ComPtr<IDXGIFactory> DXGIFactory;

					Check(this->Device.As(&DXGIDevice));

					Check(DXGIDevice->GetAdapter(DXGIAdapter.GetAddressOf()));
					Check(DXGIAdapter->GetParent(IID_PPV_ARGS(DXGIFactory.GetAddressOf())));

					Check(DXGIFactory->CreateSwapChain(this->Device.Get(), &SwapChainDesc, this->SwapChain.ReleaseAndGetAddressOf()));
					Check(DXGIFactory->MakeWindowAssociation(SwapChainDesc.OutputWindow, DXGI_MWA_NO_ALT_ENTER));
				}

				Check(this->CreateView());

				return true;
			}
			catch (const HRESULT ErrorCode)
			{
				SetLastError(ErrorCode);
			}
			return false;
		}

		void BeginDraw(float R, float G, float B, float A) override
		{
			const float ClearColor[4] = { R, G, B, A };
			DeviceContext->ClearRenderTargetView(RenderTargetView.Get(), ClearColor);
			DeviceContext->ClearDepthStencilView(DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		}

		bool EndDraw(UINT SyncInterval) override
		{
			return SUCCEEDED(SwapChain->Present(SyncInterval, 0));
		}

		void Draw(UINT IndexCount) override
		{
			this->DeviceContext->DrawIndexed(IndexCount, 0, 0);
		}
	private:
		LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			switch (uMsg)
			{
				case WM_CREATE:
				{
					constexpr RAWINPUTDEVICE RawInputDevice[] = {
						{ HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_MOUSE },
					};

					RegisterRawInputDevices(RawInputDevice, ARRAYSIZE(RawInputDevice), sizeof(RAWINPUTDEVICE));
					return 0;
				}

				case WM_INPUT:
				{
					RAWINPUT RawInput = {};
					UINT RawInputSize = sizeof(RAWINPUT);

					if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &RawInput, &RawInputSize, sizeof(RAWINPUTHEADER)) == -1)
					{
						return 0;
					}

					switch (RawInput.header.dwType)
					{
						case RIM_TYPEMOUSE:
						{
							this->Mouse.x += RawInput.data.mouse.lLastX;
							this->Mouse.y += RawInput.data.mouse.lLastY;
							break;
						}
					}
					return 0;
				}

				case WM_GETMINMAXINFO:
				{
					reinterpret_cast<LPMINMAXINFO>(lParam)->ptMinTrackSize = { 220, 170 };
					return 0;
				}

				case WM_SIZE:
				{
					const int Width = GET_X_LPARAM(lParam);
					if (Width > 0)
					{
						this->WndSize.x = Width;
					}

					const int Height = GET_Y_LPARAM(lParam);
					if (Height > 0)
					{
						this->WndSize.y = Height;
					}
					return 0;
				}

				case WM_KEYDOWN:
				{
					this->KeyState[static_cast<BYTE>(wParam)] = true;
					return 0;
				}

				case WM_KEYUP:
				{
					this->KeyState[static_cast<BYTE>(wParam)] = false;
					return 0;
				}

				case WM_DESTROY:
				{
					PostQuitMessage(0);
					return 0;
				}
			}
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
		}

		static LRESULT WINAPI StartWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			if (uMsg == WM_NCCREATE)
			{
				SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams));
				SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(RedirectWndProc));
			}
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
		}

		static LRESULT WINAPI RedirectWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			Application* const Object = reinterpret_cast<Application*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
			return Object->WndProc(hWnd, uMsg, wParam, lParam);
		}
	private:
		HRESULT CreateDevice(UINT BufferCount, UINT Width, UINT Height, UINT SampleCount)
		{
			try
			{
				Check(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, this->Device.ReleaseAndGetAddressOf(), nullptr, this->DeviceContext.ReleaseAndGetAddressOf()));

				DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
				SwapChainDesc.BufferCount = 1;
				SwapChainDesc.BufferDesc.Width = Width;
				SwapChainDesc.BufferDesc.Height = Height;
				SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
				SwapChainDesc.SampleDesc.Count = SampleCount;
				SwapChainDesc.SampleDesc.Quality = 0;
				SwapChainDesc.OutputWindow = this->hWnd;
				SwapChainDesc.Windowed = true;

				Microsoft::WRL::ComPtr<IDXGIDevice> DXGIDevice;
				Microsoft::WRL::ComPtr<IDXGIAdapter> DXGIAdapter;
				Microsoft::WRL::ComPtr<IDXGIFactory> DXGIFactory;

				Check(this->Device.As(&DXGIDevice));

				Check(DXGIDevice->GetAdapter(DXGIAdapter.GetAddressOf()));
				Check(DXGIAdapter->GetParent(IID_PPV_ARGS(DXGIFactory.GetAddressOf())));

				Check(DXGIFactory->CreateSwapChain(this->Device.Get(), &SwapChainDesc, this->SwapChain.ReleaseAndGetAddressOf()));
				Check(DXGIFactory->MakeWindowAssociation(SwapChainDesc.OutputWindow, DXGI_MWA_NO_ALT_ENTER));

				Check(this->CreateView());

				this->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			}
			catch (const HRESULT ErrorCode)
			{
				return ErrorCode;
			}
			return S_OK;
		}

		HRESULT CreateView()
		{
			try
			{
				Microsoft::WRL::ComPtr<ID3D11Texture2D> BackBuffer;
				Check(this->SwapChain->GetBuffer(0, IID_PPV_ARGS(BackBuffer.GetAddressOf())));
				Check(this->Device->CreateRenderTargetView(BackBuffer.Get(), nullptr, this->RenderTargetView.ReleaseAndGetAddressOf()));

				D3D11_TEXTURE2D_DESC BackBufferDesc = {};
				BackBuffer->GetDesc(&BackBufferDesc);

				Microsoft::WRL::ComPtr<ID3D11Texture2D> DepthStencilBuffer;
				D3D11_TEXTURE2D_DESC DepthDesc = {};
				DepthDesc.Width = BackBufferDesc.Width;
				DepthDesc.Height = BackBufferDesc.Height;
				DepthDesc.MipLevels = 1;
				DepthDesc.ArraySize = 1;
				DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
				DepthDesc.SampleDesc.Count = BackBufferDesc.SampleDesc.Count;
				DepthDesc.SampleDesc.Quality = BackBufferDesc.SampleDesc.Quality;
				DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
				Check(this->Device->CreateTexture2D(&DepthDesc, nullptr, DepthStencilBuffer.GetAddressOf()));

				D3D11_DEPTH_STENCIL_VIEW_DESC DepthStencilViewDesc = {};
				DepthStencilViewDesc.Format = DepthDesc.Format;
				DepthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
				Check(this->Device->CreateDepthStencilView(DepthStencilBuffer.Get(), &DepthStencilViewDesc, this->DepthStencilView.ReleaseAndGetAddressOf()));

				this->DeviceContext->OMSetRenderTargets(1, this->RenderTargetView.GetAddressOf(), this->DepthStencilView.Get());

				D3D11_VIEWPORT ViewPort = {};
				ViewPort.Width = static_cast<FLOAT>(BackBufferDesc.Width);
				ViewPort.Height = static_cast<FLOAT>(BackBufferDesc.Height);
				ViewPort.MinDepth = D3D11_MIN_DEPTH;
				ViewPort.MaxDepth = D3D11_MAX_DEPTH;
				this->DeviceContext->RSSetViewports(1, &ViewPort);
			}
			catch (const HRESULT ErrorCode)
			{
				return ErrorCode;
			}
			return S_OK;
		}
	private:
		const HINSTANCE hInstance = GetModuleHandle(nullptr);
		TCHAR FileName[MAX_PATH] = {};
		HWND hWnd = nullptr;
		MSG Msg = {};
		POINT WndSize = {};
		POINT Mouse = {};
		bool KeyState[0xff] = {};
	protected:
		Microsoft::WRL::ComPtr<ID3D11Device> Device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> DeviceContext;
		Microsoft::WRL::ComPtr<IDXGISwapChain> SwapChain;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> RenderTargetView;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> DepthStencilView;
	protected:
		friend class VertexShader;
		friend class PixelShader;
		friend class ConstantBuffer;
		friend class VertexBuffer;
		friend class IndexBuffer;
		friend class Texture;
	};

	std::shared_ptr<IApplication> IApplication::Create(LPCTSTR Title, INT X, INT Y, UINT Width, UINT Height, DWORD Style)
	{
		return CreateInterface<Application>(Title, X, Y, Width, Height, Style);
	}



	class VertexShader : public IVertexShader
	{
	public:
		VertexShader(Application* App, LPCVOID Data, SIZE_T Size)
		{
			Microsoft::WRL::ComPtr<ID3D11ShaderReflection> ShaderReflection;
			Check(D3DReflect(Data, Size, IID_PPV_ARGS(ShaderReflection.GetAddressOf())));

			D3D11_SHADER_DESC ShaderDesc = {};
			Check(ShaderReflection->GetDesc(&ShaderDesc));

			D3D11_INPUT_ELEMENT_DESC InputElementDesc[D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT] = {};
			for (UINT Index = 0; Index < ShaderDesc.InputParameters; Index++)
			{
				D3D11_SIGNATURE_PARAMETER_DESC SignatureParameterDesc = {};
				Check(ShaderReflection->GetInputParameterDesc(Index, &SignatureParameterDesc));

				InputElementDesc[Index].SemanticName = SignatureParameterDesc.SemanticName;
				InputElementDesc[Index].SemanticIndex = SignatureParameterDesc.SemanticIndex;
				InputElementDesc[Index].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
				InputElementDesc[Index].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;

				if (SignatureParameterDesc.Mask == 1)
				{
					constexpr DXGI_FORMAT Format[4] = { DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_SINT, DXGI_FORMAT_R32_FLOAT };
					InputElementDesc[Index].Format = Format[SignatureParameterDesc.ComponentType];
				}
				else if (SignatureParameterDesc.Mask <= 3)
				{
					constexpr DXGI_FORMAT Format[4] = { DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R32G32_UINT, DXGI_FORMAT_R32G32_SINT, DXGI_FORMAT_R32G32_FLOAT };
					InputElementDesc[Index].Format = Format[SignatureParameterDesc.ComponentType];
				}
				else if (SignatureParameterDesc.Mask <= 7)
				{
					constexpr DXGI_FORMAT Format[4] = { DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R32G32B32_UINT, DXGI_FORMAT_R32G32B32_SINT, DXGI_FORMAT_R32G32B32_FLOAT };
					InputElementDesc[Index].Format = Format[SignatureParameterDesc.ComponentType];
				}
				else if (SignatureParameterDesc.Mask <= 15)
				{
					constexpr DXGI_FORMAT Format[4] = { DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32A32_SINT, DXGI_FORMAT_R32G32B32A32_FLOAT };
					InputElementDesc[Index].Format = Format[SignatureParameterDesc.ComponentType];
				}
			}

			Check(App->Device->CreateInputLayout(InputElementDesc, ShaderDesc.InputParameters, Data, Size, this->D3D11InputLayout.GetAddressOf()));
			Check(App->Device->CreateVertexShader(Data, Size, nullptr, this->D3D11VertexShader.GetAddressOf()));
			this->D3D11DeviceContext = App->DeviceContext;
		}

		void Set() override
		{
			this->D3D11DeviceContext->IASetInputLayout(this->D3D11InputLayout.Get());
			this->D3D11DeviceContext->VSSetShader(this->D3D11VertexShader.Get(), nullptr, 0);
		}
	private:
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> D3D11DeviceContext;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> D3D11InputLayout;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> D3D11VertexShader;
	};

	std::shared_ptr<IVertexShader> IVertexShader::Create(std::shared_ptr<IApplication> App, LPCVOID Data, SIZE_T Size)
	{
		return CreateInterface<VertexShader>(static_cast<Application*>(App.get()), Data, Size);
	}



	class PixelShader : public IPixelShader
	{
	public:
		PixelShader(Application* App, LPCVOID Data, SIZE_T Size)
		{
			Check(App->Device->CreatePixelShader(Data, Size, nullptr, this->D3D11PixelShader.GetAddressOf()));
			this->D3D11DeviceContext = App->DeviceContext;
		}

		void Set() override
		{
			this->D3D11DeviceContext->PSSetShader(this->D3D11PixelShader.Get(), nullptr, 0);
		}
	private:
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> D3D11DeviceContext;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> D3D11PixelShader;
	};

	std::shared_ptr<IPixelShader> IPixelShader::Create(std::shared_ptr<IApplication> App, LPCVOID Data, SIZE_T Size)
	{
		return CreateInterface<PixelShader>(static_cast<Application*>(App.get()), Data, Size);
	}



	class ConstantBuffer : public IConstantBuffer
	{
	public:
		ConstantBuffer(Application* App, SIZE_T DataSize)
		{
			D3D11_BUFFER_DESC BufferDesc = {};
			BufferDesc.ByteWidth = DataSize;
			BufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			Check(App->Device->CreateBuffer(&BufferDesc, nullptr, this->D3D11Buffer.GetAddressOf()));
			this->D3D11DeviceContext = App->DeviceContext;
		}

		void Update(LPCVOID Data) override
		{
			this->D3D11DeviceContext->UpdateSubresource(this->D3D11Buffer.Get(), 0, nullptr, Data, 0, 0);
		}

		void SetVertexShader() override
		{
			this->D3D11DeviceContext->VSSetConstantBuffers(0, 1, this->D3D11Buffer.GetAddressOf());
		}

		void SetPixelShader() override
		{
			this->D3D11DeviceContext->PSSetConstantBuffers(0, 1, this->D3D11Buffer.GetAddressOf());
		}
	private:
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> D3D11DeviceContext;
		Microsoft::WRL::ComPtr<ID3D11Buffer> D3D11Buffer;
	};

	std::shared_ptr<IConstantBuffer> IConstantBuffer::Create(std::shared_ptr<IApplication> App, SIZE_T DataSize)
	{
		return CreateInterface<ConstantBuffer>(static_cast<Application*>(App.get()), DataSize);
	}



	class VertexBuffer : public IVertexBuffer
	{
	public:
		VertexBuffer(Application* App, LPCVOID Data, SIZE_T CountElement, SIZE_T SizeElement)
		{
			D3D11_BUFFER_DESC BufferDesc = {};
			BufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
			BufferDesc.ByteWidth = CountElement * SizeElement;
			BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

			D3D11_SUBRESOURCE_DATA SubresourceData = {};
			SubresourceData.pSysMem = Data;

			Check(App->Device->CreateBuffer(&BufferDesc, &SubresourceData, this->D3D11Buffer.GetAddressOf()));
			this->Stride = SizeElement;
			this->D3D11DeviceContext = App->DeviceContext;
		}

		void Set() override
		{
			UINT Offset = 0;
			this->D3D11DeviceContext->IASetVertexBuffers(0, 1, this->D3D11Buffer.GetAddressOf(), &this->Stride, &Offset);
		}
	private:
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> D3D11DeviceContext;
		Microsoft::WRL::ComPtr<ID3D11Buffer> D3D11Buffer;

		UINT Stride = 0;
	};

	std::shared_ptr<IVertexBuffer> IVertexBuffer::Create(std::shared_ptr<IApplication> App, LPCVOID Data, SIZE_T CountElement, SIZE_T SizeElement)
	{
		return CreateInterface<VertexBuffer>(static_cast<Application*>(App.get()), Data, CountElement, SizeElement);
	}



	class IndexBuffer : public IIndexBuffer
	{
	public:
		IndexBuffer(Application* App, LPCVOID Data, SIZE_T CountElement)
		{
			D3D11_BUFFER_DESC BufferDesc = {};
			BufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
			BufferDesc.ByteWidth = CountElement * sizeof(UINT);
			BufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

			D3D11_SUBRESOURCE_DATA SubresourceData = {};
			SubresourceData.pSysMem = Data;

			Check(App->Device->CreateBuffer(&BufferDesc, &SubresourceData, this->D3D11Buffer.GetAddressOf()));

			this->Count = CountElement;

			this->D3D11DeviceContext = App->DeviceContext;
		}

		void Set() override
		{
			this->D3D11DeviceContext->IASetIndexBuffer(this->D3D11Buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		}

		unsigned int GetIndexCount() override
		{
			return this->Count;
		}
	private:
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> D3D11DeviceContext;
		Microsoft::WRL::ComPtr<ID3D11Buffer> D3D11Buffer;

		UINT Count = 0;
	};

	std::shared_ptr<IIndexBuffer> IIndexBuffer::Create(std::shared_ptr<IApplication> App, LPCVOID Data, SIZE_T CountElement)
	{
		return CreateInterface<IndexBuffer>(static_cast<Application*>(App.get()), Data, CountElement);
	}



	class Texture : public ITexture
	{
	public:
		Texture(Application* App, LPCVOID Data, UINT Width, UINT Height)
		{
			Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;

			D3D11_TEXTURE2D_DESC TextureDesc = {};
			TextureDesc.Width = Width;
			TextureDesc.Height = Height;
			TextureDesc.MipLevels = 0;
			TextureDesc.ArraySize = 1;
			TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			TextureDesc.SampleDesc.Count = 1;
			TextureDesc.SampleDesc.Quality = 0;
			TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
			TextureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
			Check(App->Device->CreateTexture2D(&TextureDesc, nullptr, Texture.GetAddressOf()));

			App->DeviceContext->UpdateSubresource(Texture.Get(), 0, nullptr, Data, Width * 4, 0);

			D3D11_SHADER_RESOURCE_VIEW_DESC ShaderResourceViewDesc = {};
			ShaderResourceViewDesc.Format = TextureDesc.Format;
			ShaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
			Check(App->Device->CreateShaderResourceView(Texture.Get(), &ShaderResourceViewDesc, this->D3D11ShaderResourceView.GetAddressOf()));
			App->DeviceContext->GenerateMips(D3D11ShaderResourceView.Get());

			this->D3D11DeviceContext = App->DeviceContext;
		}

		void Set() override
		{
			this->D3D11DeviceContext->PSSetShaderResources(0, 1, this->D3D11ShaderResourceView.GetAddressOf());
		}
	private:
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> D3D11DeviceContext;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> D3D11ShaderResourceView;
	};

	std::shared_ptr<ITexture> ITexture::Create(std::shared_ptr<IApplication> App, LPCVOID Data, UINT Width, UINT Height)
	{
		return CreateInterface<Texture>(static_cast<Application*>(App.get()), Data, Width, Height);
	}
}
