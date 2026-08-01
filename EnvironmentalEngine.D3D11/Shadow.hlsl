cbuffer PerObject : register(b1)
{
    float4x4 transform;
};

float4 VSMain(float3 pos : POSITION) : SV_Position
{
    return mul(float4(pos, 1.0f), transform);
}