#ifndef _ENGINE_H_
#define _ENGINE_H_

#include <tchar.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <memory>

namespace Engine
{
	float GetTimer();

	class IApplication
	{
	public:
		static std::shared_ptr<IApplication> Create(LPCTSTR Title, INT X, INT Y, UINT Width, UINT Height, DWORD Style = WS_OVERLAPPEDWINDOW);
		virtual bool Run() = 0;
		virtual void Quit() = 0;
		virtual int GetWidth() const = 0;
		virtual int GetHeight() const = 0;
		virtual int GetMouseDirectionX() const = 0;
		virtual int GetMouseDirectionY() const = 0;
		virtual bool GetKeyState(BYTE KeyCode) const = 0;
		virtual bool ResizeBuffer(UINT BufferCount, UINT Width, UINT Height, UINT SampleCount) = 0;
		virtual void BeginDraw(float R, float G, float B, float A) = 0;
		virtual bool EndDraw(UINT SyncInterval) = 0;
		virtual void Draw(UINT IndexCount) = 0;
		virtual ~IApplication() = default;
	};

	class IVertexShader
	{
	public:
		static std::shared_ptr<IVertexShader> Create(std::shared_ptr<IApplication> App, LPCVOID Data, SIZE_T Size);
		virtual void Set() = 0;
		virtual ~IVertexShader() = default;
	};

	class IPixelShader
	{
	public:
		static std::shared_ptr<IPixelShader> Create(std::shared_ptr<IApplication> App, LPCVOID Data, SIZE_T Size);
		virtual void Set() = 0;
		virtual ~IPixelShader() = default;
	};

	class IConstantBuffer
	{
	public:
		static std::shared_ptr<IConstantBuffer> Create(std::shared_ptr<IApplication> App, SIZE_T Size);
		virtual void Update(LPCVOID Data) = 0;
		virtual void SetVertexShader() = 0;
		virtual void SetPixelShader() = 0;
		virtual ~IConstantBuffer() = default;
	};

	class IVertexBuffer
	{
	public:
		static std::shared_ptr<IVertexBuffer> Create(std::shared_ptr<IApplication> App, LPCVOID Data, SIZE_T CountElement, SIZE_T SizeElement);
		virtual void Set() = 0;
		virtual ~IVertexBuffer() = default;
	};

	class IIndexBuffer
	{
	public:
		static std::shared_ptr<IIndexBuffer> Create(std::shared_ptr<IApplication> App, LPCVOID Data, SIZE_T CountElement);
		virtual void Set() = 0;
		virtual unsigned int GetIndexCount() = 0;
		virtual ~IIndexBuffer() = default;
	};

	class ITexture
	{
	public:
		static std::shared_ptr<ITexture> Create(std::shared_ptr<IApplication> App, LPCVOID Data, UINT Width, UINT Height);
		virtual void Set() = 0;
		virtual ~ITexture() = default;
	};
}

#endif