cbuffer FrameConstants : register(b0)
{
    float4x4 transform;
    float4x4 world;
    float4x4 normal;
    float3 camPos;
    float padding0;
    float4 cubeColor;
};

struct VSInput
{
    float3 position : POSITION;      
    float3 normal : NORMAL;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal : NORMAL;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    float4 position = float4(input.position, 1.0);
    
    output.position = mul(position, transform);
    output.worldPos = mul(position, world).xyz;
    output.normal = normalize(mul(float4(input.normal, 0.0), normal));
    return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
        float3 N = normalize(input.normal);
    
        float4 ambientColor = float4(1.0f, 1.0f, 1.0f, 0.1f);
        float4 lightColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
        float3 lightDir = normalize(float3(1.0f, 0.0f, 0.0f));
    
        float diff = max(dot(N, lightDir), 0.0);
    
        float3 viewDir = normalize(camPos - input.worldPos);
        float3 reflectedDir = reflect(-lightDir, N);
    
        float spec = pow(max(dot(viewDir, reflectedDir), 0.0), 128.0);
        spec *= step(0.0, dot(input.normal, lightDir));
    
        float3 ambient = float3(ambientColor.rgb * ambientColor.a);
        float3 diffuse = lightColor.rgb * diff * lightColor.a;
        float3 specular = lightColor.rgb * spec * 0.5;
    
        return float4(cubeColor.rgb * (ambient + diffuse) + specular, 1.0);
    
}