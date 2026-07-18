cbuffer FrameConstants : register(b0)
{
    float4x4 transform;
};

struct VSInput
{
    float3 position : POSITION;      
    float3 color : COLOR;
    float3 normal : NORMAL;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float3 normal : NORMAL;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    output.position = mul(float4(input.position, 1.0), transform);
    output.color = input.color;
    output.normal = input.normal;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float4 ambientColor = float4(1.0f, 0.5f, 0.0f, 1.0f);
    float3 lightDir = normalize(float3(1.0f, 0.2f, 0.3f));
    //return float4(input.color * float3(ambientColor.rgb * ambientColor.a), 1.0);
    return float4(input.normal, 1.0f);
}