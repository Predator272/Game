#ifndef _HELPER_H_
#define _HELPER_H_

#include "include/engine.h"
#include "tiny_obj_loader.h"
#include <wrl/client.h>



LONG WINAPI VectoredExceptionHandler(struct _EXCEPTION_POINTERS* ExceptionInfo)
{
	const DWORD Code = ExceptionInfo->ExceptionRecord->ExceptionCode;

	constexpr TCHAR Hex[] = { _T('0'), _T('1'), _T('2'), _T('3'), _T('4'), _T('5'), _T('6'), _T('7'), _T('8'), _T('9'), _T('A'), _T('B'), _T('C'), _T('D'), _T('E'), _T('F') };

	const static TCHAR String[] = {
		_T('E'), _T('r'), _T('r'), _T('o'), _T('r'), _T(':'), _T(' '), _T('0'), _T('x'),
		Hex[((Code >> 28) & 0x0F)], Hex[((Code >> 24) & 0x0F)], Hex[((Code >> 20) & 0x0F)], Hex[((Code >> 16) & 0x0F)],
		Hex[((Code >> 12) & 0x0F)], Hex[((Code >> 8) & 0x0F)], Hex[((Code >> 4) & 0x0F)], Hex[((Code >> 0) & 0x0F)],
		_T('.'), _T('\0')
	};

	FatalAppExit(0, String);

	return EXCEPTION_EXECUTE_HANDLER;
}

inline void Assert(HRESULT ErrorCode)
{
	if (FAILED(ErrorCode))
	{
		RaiseException(ErrorCode, 0, 0, nullptr);
	}
}

namespace ResourceLoader
{
	class Image
	{
	public:
		bool Load(const void* Data, size_t Size)
		{
			this->Width = 0;
			this->Height = 0;
			this->Color.clear();

			if (Data == nullptr) return false;

			if (Size < 18) return false;

			uint8_t* BeginPtr = ((uint8_t*)Data);
			uint8_t* EndPtr = ((uint8_t*)Data) + Size;

			const uint8_t IDLength = Read8(BeginPtr, EndPtr);
			const uint8_t ColorMapType = Read8(BeginPtr, EndPtr);
			const uint8_t DataType = Read8(BeginPtr, EndPtr);

			const uint16_t ColorMapOrigin = Read16(BeginPtr, EndPtr);
			const uint16_t ColorMapLength = Read16(BeginPtr, EndPtr);
			const uint8_t ColorMapDepth = Read8(BeginPtr, EndPtr);

			const uint16_t OriginX = Read16(BeginPtr, EndPtr);
			const uint16_t OriginY = Read16(BeginPtr, EndPtr);
			const uint16_t Width = Read16(BeginPtr, EndPtr);
			const uint16_t Height = Read16(BeginPtr, EndPtr);
			const uint8_t BitsPerPixel = Read8(BeginPtr, EndPtr);
			const uint8_t Attribute = Read8(BeginPtr, EndPtr);

			if (DataType != 2 && DataType != 10) return false;

			if (BitsPerPixel != 24 && BitsPerPixel != 32) return false;

			if (ColorMapType != 0) return false;

			if (Width == 0 || Height == 0) return false;

			this->Width = Width;
			this->Height = Height;
			this->Color.resize(this->Width * this->Height);

			const bool Alpha = (BitsPerPixel == 32);
			const size_t ImageSize = Width * Height;
			size_t Index = 0;

			if (DataType == 2)
			{
				while (Index < ImageSize)
				{
					const uint32_t Color = ReadPixel(BeginPtr, EndPtr, Alpha);
					this->Color[Index] = Color;
					Index++;
				}
			}
			else
			{
				while (Index < ImageSize)
				{
					const uint8_t BlockInfo = Read8(BeginPtr, EndPtr);
					uint8_t PixelCount = (BlockInfo & 0x7F) + 1;
					if (BlockInfo & 0x80)
					{
						const uint32_t Color = ReadPixel(BeginPtr, EndPtr, Alpha);
						while (PixelCount-- > 0 && Index < ImageSize)
						{
							this->Color[Index] = Color;
							Index++;
						}
					}
					else
					{
						while (PixelCount-- > 0 && Index < ImageSize)
						{
							const uint32_t Color = ReadPixel(BeginPtr, EndPtr, Alpha);
							this->Color[Index] = Color;
							Index++;
						}
					}
				}
			}

			return true;
		}

		uint16_t GetWidth() const
		{
			return this->Width;
		}

		uint16_t GetHeight() const
		{
			return this->Width;
		}

		const uint32_t* GetColor() const
		{
			return this->Color.data();
		}

	private:
		uint8_t Read8(uint8_t*& Begin, uint8_t*& End)
		{
			if (Begin < End)
			{
				return *(Begin++);
			}
			return 0;
		}

		uint16_t Read16(uint8_t*& Begin, uint8_t*& End)
		{
			const uint8_t Low = Read8(Begin, End);
			return Low | (Read8(Begin, End) << 8);
		}

		uint32_t ReadPixel(uint8_t*& Begin, uint8_t*& End, bool Alpha)
		{
			const uint8_t R = Read8(Begin, End);
			const uint8_t G = Read8(Begin, End);
			const uint8_t B = Read8(Begin, End);
			const uint8_t A = (Alpha ? Read8(Begin, End) : 0xFF);
			return (A << 24) | (R << 16) | (G << 8) | B;
		}

	private:
		uint16_t Width = 0;
		uint16_t Height = 0;
		std::vector<uint32_t> Color;
	};

	struct VERTEX_STRUCT
	{
	public:
		float x, y, z, tx, ty, nx, ny, nz;
	};

	class Model
	{
	public:
		bool Load(const char* FileName)
		{
			this->Vertex.clear();
			this->Index.clear();

			OBJLOADER::attrib_t Attrib;
			std::vector<OBJLOADER::shape_t> Shape;
			std::vector<OBJLOADER::material_t> Material;

			if (!OBJLOADER::LoadObj(&Attrib, &Shape, &Material, nullptr, FileName))
			{
				return false;
			}

			for (const auto& shape : Shape)
			{
				for (const auto& index : shape.mesh.indices)
				{
					const VERTEX_STRUCT Temp = {
						Attrib.vertices[3 * index.Position + 0],
						Attrib.vertices[3 * index.Position + 1],
						Attrib.vertices[3 * index.Position + 2],

						Attrib.texcoords[2 * index.TexCoord + 0],
						Attrib.texcoords[2 * index.TexCoord + 1],

						Attrib.normals[3 * index.Normal + 0],
						Attrib.normals[3 * index.Normal + 1],
						Attrib.normals[3 * index.Normal + 2],
					};

					this->Vertex.push_back(Temp);

					this->Index.push_back(this->Index.size());

					const size_t IndexCount = this->Index.size();
				}
			}

			return true;
		}

		const VERTEX_STRUCT* GetVertex() const
		{
			return this->Vertex.data();
		}

		size_t GetVertexCount() const
		{
			return this->Vertex.size();
		}

		const uint32_t* GetIndex() const
		{
			return this->Index.data();
		}

		size_t GetIndexCount() const
		{
			return this->Index.size();
		}

	private:
		std::vector<VERTEX_STRUCT> Vertex;
		std::vector<uint32_t> Index;
	};
}

#endif
