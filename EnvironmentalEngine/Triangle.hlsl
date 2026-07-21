cbuffer FrameConstants : register(b0)
{
    float offsetX;
    float offsetY;
    float padding[2];
}

struct VSInput
{
    float3 position : POSITION;      
    float3 color : COLOR;
    float elevation : ELEVATION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 color : COLOR;
    float elevation : ELEVATION;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    output.position = float4(input.position.x + offsetX,
                             input.position.y + offsetY,
                             input.position.z,
                             1.0);
    output.color = input.color;
    output.elevation = input.elevation;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
    if(input.elevation < 0.0f) return 
        float4(input.color, 1.0f);
    else
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
}