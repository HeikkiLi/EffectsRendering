// shadowmap texture
Texture2D<float> ShadowMapTexture : register(t0);

// point sampler
SamplerState PointSampler : register(s0);

cbuffer cbShadowMapVis : register(b0)
{
	float2 DepthScaleOffset : packoffset(c0);
}

static const float2 arrPos[4] = {
  float2(1.0, 1.0),
  float2(1.0, -1.0),
  float2(-1.0, 1.0),
  float2(-1.0, -1.0),
};

static const float2 arrUV[4] = {
  float2(1.0, 0.0),
  float2(1.0, 1.0),
  float2(0.0, 0.0),
  float2(0.0, 1.0),
};

struct VS_OUTPUT
{
	float4 Position : SV_Position; // vertex position 
	float2 UV		: TEXCOORD0;   // vertex texture coords
};

VS_OUTPUT ShadowMapVisVS(uint VertexID : SV_VertexID)
{
	VS_OUTPUT Output;

	Output.Position = float4(arrPos[VertexID].xy * 0.3 + float2(0.6, 0.6), 0.0, 1.0);
	Output.UV = arrUV[VertexID].xy;

	return Output;
}

float4 ShadowMapVisPS(VS_OUTPUT In) : SV_TARGET
{
	
	float4 depth = ShadowMapTexture.Gather(PointSampler, In.UV.xy);
	float DDepth = dot(abs(depth.xxx - depth.yzw), float3(1.0, 1.0, 1.0));
	float finalDepth = depth.x + DepthScaleOffset.x * saturate(DDepth - DepthScaleOffset.y);

	return float4(finalDepth, finalDepth, finalDepth, 1.0);
}