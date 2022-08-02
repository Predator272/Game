#include "helper.hlsli"

Texture2D Texture;
SamplerState Sampler;

float4 main(PS_STRUCT Input) : SV_TARGET
{
	return Texture.Sample(Sampler, Input.TexCoord);
}