cbuffer FrameConstants : register(b0)
{
    float4x4 transform;
    float4x4 world;
    float4x4 normal;
    float3 camPos;
    float padding0;
    float4 cubeColor;
    float3 ambientColor;
    float ambientIntensity;
    float3 lightColor;
    float specularIntensity;
    float smoothness;
    float3 lightDirection;
    float3 pLightPosition;
    float pIntensity;
    float3 pColor;
    float padding1;
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
    
    float3 lightDir = normalize(-lightDirection);
    
    float diff = max(dot(N, lightDir), 0.0);
    
    float3 viewDir = normalize(camPos - input.worldPos);
    float3 reflectedViewDir = reflect(viewDir, N);
    
    float spec = pow(max(dot(lightDir, -reflectedViewDir), 0.0), max(smoothness * 512.0, 1.0));
    spec *= step(0.0, dot(input.normal, lightDir));
    
    float3 pLightDir = normalize(pLightPosition - input.worldPos);
    float pDiff = max(dot(N, pLightDir), 0.0);
    float pSpec = pow(max(dot(pLightDir, -reflectedViewDir), 0.0), max(smoothness * 512.0, 1.0));
    pSpec *= step(0.0, dot(input.normal, pLightDir));
    
    float3 pDiffuse = pColor.rgb * pDiff;
    float3 pSpecular = pColor.rgb * pSpec * specularIntensity;
    float pFalloff = min(1.0 / pow(distance(input.worldPos, pLightPosition), 2), 1.0);
    
    
    float3 ambient = float3(ambientColor.rgb * ambientIntensity);
    float3 diffuse = lightColor.rgb * diff;
    float3 specular = lightColor.rgb * spec * specularIntensity;
    
    return float4(
    (cubeColor.rgb * (ambient + diffuse) + specular) + (cubeColor.rgb * pDiffuse + pSpecular) * pIntensity * pFalloff
    , 1.0);
    
}