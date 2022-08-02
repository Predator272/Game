#ifndef _HELPER_HLSL_
#define _HELPER_HLSL_

struct VS_STRUCT
{
	float3 Position : POSITION;
	float2 TexCoord : TEXCOORD;
	float3 Normal : NORMAL;
};

struct PS_STRUCT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD;
	float3 Normal : NORMAL;
};

#endif // _HELPER_H_