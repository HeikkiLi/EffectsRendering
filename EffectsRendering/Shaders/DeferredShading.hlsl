#include "Common.hlsl"

// Constant Buffers

// Model vertex shader constants
cbuffer cbPerObjectVS : register(b0) 
{
    float4x4 WorldViewProjection    : packoffset(c0);
    float4x4 World                  : packoffset(c4);
}

// Model pixel shader constants
cbuffer cbPerObjectPS : register(b1) 
{
	float4 diffuseColor			: packoffset(c0);
    float specExp				: packoffset(c1.x);
    float specIntensity			: packoffset(c1.y);
	bool useDiffuseTexture		: packoffset(c1.z);
    bool useNormalMapTexture    : packoffset(c1.w);
    bool useBumpMapTexture      : packoffset(c2.x); 
}

//Textures and linear sampler
Texture2D DiffuseTexture    : register(t0);
Texture2D NormalMapTexture  : register(t1);
Texture2D BumpMapTexture    : register(t2);

// shader input/output structure
struct VS_INPUT
{
    float4 Position : POSITION;
    float3 Normal   : NORMAL;
    float2 UV       : TEXCOORD0;
    float3 TangentL : TANGENT;
};

struct VS_OUTPUT
{
    float4 Position     : SV_POSITION;
    float2 UV           : TEXCOORD0;
    float3 Normal       : TEXCOORD1;
    float3 TangentW     : TEXCOORD2;
    float3 BitangentW   : TEXCOORD3;
};

// Vertex shader
VS_OUTPUT RenderSceneVS(VS_INPUT input)
{
    VS_OUTPUT Output;

    // Transform position from object space to homogeneous projection space
    Output.Position = mul(input.Position, WorldViewProjection);
    Output.UV = input.UV;

    // Transform normal & tangent to world space
    Output.Normal = mul(input.Normal, (float3x3)World);
    Output.TangentW = mul(input.TangentL, (float3x3)World);

    // Compute bitangent in world space
    Output.BitangentW = cross(Output.Normal, Output.TangentW);
    
    return Output;
}


// Pixel shader
// GBUFFER OUT struct
struct PS_GBUFFER_OUT
{
    float4 ColorSpecInt : SV_TARGET0;
    float4 Normal       : SV_TARGET1;
    float4 SpecPow      : SV_TARGET2;
};


// Packs all the data into the GBuffer structure
PS_GBUFFER_OUT PackGBuffer(float3 BaseColor, float3 Normal, float SpecIntensity, float SpecPower)
{
    PS_GBUFFER_OUT Out;

	// Normalize the specular power
    float SpecPowerNorm = max(0.0001, (SpecPower - g_SpecPowerRange.x) / g_SpecPowerRange.y);

    Out.ColorSpecInt = float4(BaseColor.rgb, SpecIntensity);
    Out.Normal = float4(Normal * 0.5 + 0.5, 0.0);
    Out.SpecPow = float4(SpecPowerNorm, 0.0, 0.0, 0.0);

    return Out;
}

PS_GBUFFER_OUT RenderScenePS(VS_OUTPUT In)
{
    // Sample diffuse texture
    float3 DiffuseColor = diffuseColor.xyz;
    if (useDiffuseTexture)
    {
        DiffuseColor *= DiffuseTexture.Sample(LinearSampler, In.UV).rgb;
    }

    // Default normal is from vertex (world space)
    float3 normal = normalize(In.Normal);

    // Apply normal mapping if available
    if (useNormalMapTexture)
    {
        // Sample normal map
        float3 normalMapSample = NormalMapTexture.Sample(LinearSampler, In.UV).rgb;

        // Convert the normal map from tangent space to world space
        normal = NormalSampleToWorldSpace(normalMapSample, In.Normal, In.TangentW);
        normal *= 0.5; // Reduce intensity
    }
    // Handle bump mapping if normal map is unavailable
    else if (useBumpMapTexture && !useNormalMapTexture)
    {
        // Get texture dimensions
        uint width, height;
        BumpMapTexture.GetDimensions(width, height);
        float2 texelSize = 1.0 / float2(width, height);

        // Sample bump map in three locations (center, X, Y)
        float bumpHeight1 = BumpMapTexture.Sample(LinearSampler, In.UV).r;  // Current texel
        float bumpHeightX = BumpMapTexture.Sample(LinearSampler, In.UV + float2(texelSize.x, 0)).r; // Neighbor texel in X
        float bumpHeightY = BumpMapTexture.Sample(LinearSampler, In.UV + float2(0, texelSize.y)).r; // Neighbor texel in Y

        // Compute X and Y gradients
        float dx = bumpHeightX - bumpHeight1;
        float dy = bumpHeightY - bumpHeight1;

        // Create tangent-space normal from gradients
        float3 tangentSpaceNormal = normalize(float3(dx, dy, 1.0));

        // Transform from tangent space to world space using TBN matrix
        float3x3 TBN = float3x3(normalize(In.TangentW), normalize(In.BitangentW), normalize(In.Normal));
        normal = normalize(mul(tangentSpaceNormal, TBN));
    }


    // Pack and return GBuffer data
    return PackGBuffer(DiffuseColor, normal, specIntensity, specExp);
}
