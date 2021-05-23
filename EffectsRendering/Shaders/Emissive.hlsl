cbuffer cbEmissive : register(b2)
{
    matrix g_mWorldViewProjection : packoffset(c0);
    float4 g_color : packoffset(c4);
}

struct VS_INPUT
{
    float4 Position : POSITION; // vertex position 
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION; // vertex position 
};


VS_OUTPUT RenderEmissiveVS(VS_INPUT input)
{
    VS_OUTPUT Output;

    Output.Position = mul(input.Position, g_mWorldViewProjection);

    return Output;
}


float4 RenderEmissivePS(VS_OUTPUT In) : SV_TARGET0
{
    return g_color;
}
