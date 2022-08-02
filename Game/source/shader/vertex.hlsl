#include "helper.hlsli"

cbuffer ConstantBuffer
{
	matrix World;
	matrix View;
	matrix Projection;
}

PS_STRUCT main(VS_STRUCT Input)
{
	PS_STRUCT Output;
	Output.Position = mul(float4(Input.Position, 1.0f), World);
	Output.Position = mul(Output.Position, View);
	Output.Position = mul(Output.Position, Projection);
	Output.TexCoord = Input.TexCoord;
	Output.Normal = Input.Normal;
	return Output;
}
