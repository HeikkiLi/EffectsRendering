#include "Common.hlsl"

static float2 arrOffsets[4] = {
	float2(-0.75, -0.75),
	float2(-0.25, -0.75),
	float2(0.25, -0.75),
	float2(0.75, -0.75),
};

static const float2 arrBasePos[4] = {
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

static const float4 arrMask[4] = {
	float4(1.0, 0.0, 0.0, 0.0),
	float4(0.0, 1.0, 0.0, 0.0),
	float4(0.0, 0.0, 1.0, 0.0),
	float4(0.0, 0.0, 0.0, 1.0),
};

struct VS_OUTPUT
{
	float4 Position	: SV_Position; // vertex position 
	float2 UV		: TEXCOORD0;   // vertex texture coords
	float4 sampMask	: TEXCOORD1;
};

VS_OUTPUT GBufferVisVS(uint VertexID : SV_VertexID)
{
	VS_OUTPUT Output;
	Output.Position = float4(arrBasePos[VertexID % 4].xy * 0.2 + arrOffsets[VertexID / 4], 0.0, 1.0);
	Output.UV = arrUV[VertexID % 4].xy;
	Output.sampMask = arrMask[VertexID / 4].xyzw;

	return Output;
}


float4 GBufferVisPS(VS_OUTPUT In) : SV_TARGET
{
    SURFACE_DATA gbd = UnpackGBuffer(In.UV.xy);
    float4 finalColor = float4(0.0, 0.0, 0.0, 1.0);
    finalColor += float4(1.0 - saturate(gbd.LinearDepth / 75.0), 1.0 - saturate(gbd.LinearDepth / 125.0), 1.0 - saturate(gbd.LinearDepth / 200.0), 0.0) * In.sampMask.xxxx;
    finalColor += float4(gbd.Color.xyz, 0.0) * In.sampMask.yyyy;
    finalColor += float4(gbd.Normal.xyz * 0.5 + 0.5, 0.0) * In.sampMask.zzzz;
    finalColor += float4(gbd.SpecIntensity, gbd.SpecPow, 0.0, 0.0) * In.sampMask.wwww;

    return finalColor;
}

VS_OUTPUT TextureVisVS(uint VertexID : SV_VertexID)
{
    VS_OUTPUT Output;

    Output.Position = float4(arrBasePos[VertexID].xy, 0.0, 1.0);
    Output.UV = arrUV[VertexID].xy;
    Output.sampMask = 0;
    
    return Output;
}

float4 TextureVisDepthPS(VS_OUTPUT In) : SV_TARGET
{
	float4 finalColor = float4(0.0, 0.0, 0.0, 1.0);

	float depth = DepthTexture.Sample(PointSampler, In.UV.xy).x;
	float linearDepth = ConvertZToLinearDepth(depth);

	finalColor += float4(1.0 - saturate(linearDepth / 75.0), 1.0 - saturate(linearDepth / 125.0), 1.0 - saturate(linearDepth / 200.0), 0.0);
	
	return finalColor;
}

float4 TextureVisCSpecPS(VS_OUTPUT In) : SV_TARGET
{
	float4 baseColorSpecInt = ColorSpecIntTexture.Sample(PointSampler, In.UV.xy);
	return float4(baseColorSpecInt.xyz, 1.0);
}

float4 TextureVisNormalPS(VS_OUTPUT In) : SV_TARGET
{
	float3 normal = NormalTexture.Sample(PointSampler, In.UV.xy).xyz;
	normal = normalize(normal * 2.0 - 1.0);
	
	return float4(normal.xyz * 0.5 + 0.5, 1.0);
}

float4 TextureVisSpecPowPS(VS_OUTPUT In) : SV_TARGET
{
	float4 baseColorSpecInt = ColorSpecIntTexture.Sample(PointSampler, In.UV.xy);
	float specIntensity = baseColorSpecInt.w;
	float specPow = SpecPowTexture.Sample(PointSampler, In.UV.xy).x;
	
	return float4(specIntensity, specPow, 0.0, 1.0);
}