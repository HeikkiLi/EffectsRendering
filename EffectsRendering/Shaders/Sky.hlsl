#include "common.hlsl"

cbuffer cbPerFrameVS : register(b0)
{
	float4x4 gWorldViewProj : packoffset(c0);
};


SamplerState samTriLinearSam
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

struct VertexIn
{
	float3 PosL : POSITION;
};

struct VertexOut
{
	float4 PosH : SV_POSITION;
	float3 PosL : POSITION;
};

// Sky vertex shader
VertexOut SkyVS(VertexIn vin)
{
	VertexOut vout;

	// Set z = w so that z/w = 1 (i.e., skydome always on far plane).
	vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj).xyww;

	// Local vertex position as cubemap lookup vector.
	vout.PosL = vin.PosL;

	return vout;
}

// pixel shader sample cubemap texture
float4 SkyPS(VertexOut pin) : SV_Target
{
	return gCubeMap.Sample(samTriLinearSam, pin.PosL);
}