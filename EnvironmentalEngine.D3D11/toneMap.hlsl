cbuffer tonemapConstants : register(b0)
{
    float exposure;
    float3 padding;
}

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

Texture2D hdrTex : register(t0);

//https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

VSOutput VSMain(uint id : SV_VertexID)
{
    VSOutput o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.uv = uv;
    o.position = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float3 hdr = hdrTex.Load(int3(input.position.xy, 0));
    return float4(ACESFilm(hdr * exposure), 1.0);
}