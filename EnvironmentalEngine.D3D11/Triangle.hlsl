cbuffer PerFrameConstants : register(b0)
{
    float3 camPos;
    float padding0;
    float3 ambientColor;
    float ambientIntensity;
    float3 lightColor;
    float padding1;
    float3 lightDirection;
    float padding2;
    float3 pLightPosition;
    float pIntensity;
    float3 pColor;
    float padding3;
};

cbuffer PerObjectConstants : register(b1)
{
    float4x4 transform;
    float4x4 world;
    float4x4 normal;
    float4 cubeColor;
    float specularIntensity;
    float smoothness;
    float padding[2];
}

struct VSInput
{
    float3 position : POSITION;      
    float3 normal : NORMAL;
    float elevation : ELEVATION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal : NORMAL;
    float elevation : ELEVATION;
};

float3 get_color_from_elevation(float elevation)
{
    
    float e = saturate(elevation);
    
    float3 deepWater = float3(0.043f, 0.106f, 0.42f);
    float3 water = float3(0.467f, 0.678f, 0.878f);
    float3 sand = float3(0.875f, 0.878f, 0.467f);
    float3 grass = float3(0.11f, 0.49f, 0.2f);
    float3 rock = float3(0.365f, 0.369f, 0.4f);
    float3 snow = float3(0.918f, 0.925f, 0.961f);
    
    float3 finalColor = deepWater;
    finalColor = lerp(finalColor, water, smoothstep(0.0f, 0.28f, e));
    finalColor = lerp(finalColor, sand, smoothstep(0.48f, 0.5f, e));
    finalColor = lerp(finalColor, grass, smoothstep(0.5f, 0.6f, e));
    finalColor = lerp(finalColor, rock, smoothstep(0.64f, 0.7f, e));
    finalColor = lerp(finalColor, snow, smoothstep(0.95f, 0.96f, e));
    
    return finalColor;
}

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    float4 position = float4(input.position, 1.0);
    
    output.position = mul(position, transform);
    output.worldPos = mul(position, world).xyz;
    output.normal = normalize(mul(float4(input.normal, 0.0), normal));
    output.elevation = input.elevation;
    return output;
}

float4 PSMain(VSOutput input) : SV_Target
{
    float3 objectColor;
    if (input.elevation < 0.0f)
        objectColor = cubeColor.rgb;
    else
        objectColor = get_color_from_elevation(input.elevation);
    
    
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
    
    
    return float4((objectColor.rgb * (ambient + diffuse) + specular) + (objectColor.rgb * pDiffuse + pSpecular) * pIntensity * pFalloff
    , 1.0);
}