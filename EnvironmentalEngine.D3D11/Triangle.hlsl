cbuffer FrameConstants : register(b0)
{
    float4x4 transform;
};

struct VSInput
{
    float3 position : POSITION;      
    float3 color : COLOR;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    output.position = mul(float4(input.position, 1.0), transform);
    output.color = input.color;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
    return float4(input.color, 1.0);
}