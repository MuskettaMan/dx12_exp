cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
};

struct VertexIn
{
    float3 PosL : POSITION;
    float3 Tangent : TANGENT;
    float3 Normal : NORMAL;
    float2 Tex0 : TEX0;
    float2 Tex1 : TEX1;
    float4 Color : COLOR;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float4 Color : COLOR;
};


VertexOut VS(VertexIn vIn)
{
    VertexOut vOut;
    vOut.PosH = mul(float4(vIn.PosL, 1.0f), gWorldViewProj);
    vOut.Color = vIn.Color;

    return vOut;
}

float4 PS(VertexOut vOut) : SV_TARGET
{
    return vOut.Color;
}
