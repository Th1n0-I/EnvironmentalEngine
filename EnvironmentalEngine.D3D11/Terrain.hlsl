Texture2D biomeLUT : register(t0);
SamplerState biomeSamp : register(s0);

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

cbuffer PerPlanetConstants : register(b1)
{
    float4x4 transform;
    float4x4 world;
    float4x4 normal;
    float4 cubeColor;
    float specularIntensity;
    float smoothness;
    float percipitationThingy;
    float padding;
}

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float elevation : ELEVATION;
    float temp : TEMPERATURE;
    float perp : PERCIPITATION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal : NORMAL;
    float elevation : ELEVATION;
    float temp : TEMPERATURE;
    float perp : PERCIPITATION;
};

float3 get_color_from_elevation(float elevation)
{
    
    float e = saturate(elevation);
    
    float3 ocean = float3(1.0, 0.0, 0.0);
    float3 plains = float3(0.0, 1.0, 0.0);
    float3 mountains = float3(0.0, 0.0 , 1.0);
    
    
    float oceanWeight = 1 - smoothstep(0.38, 0.42, e);
    float mountainWeight = smoothstep(0.64, 0.72, e);
    float plainsWeight = smoothstep(0.38, 0.42, e) * (1 - smoothstep(0.64, 0.72, e));
    
    
    float weightSum = oceanWeight + mountainWeight + plainsWeight;
    
    float3 finalColor = (ocean * oceanWeight + plains * plainsWeight + mountains * mountainWeight) / weightSum;
    
    return finalColor;
}

float3 get_color_from_percipitation(float percipitation)
{
    float3 finalColor = (percipitation);
    return finalColor;
}

float3 get_color_from_temperature(float temperature)
{
    float3 finalColor = (temperature);
    return finalColor;
}

float get_percipitation_from_temperature(float percipitation, float temperature)
{
    float perp = percipitation * 0.5 + 0.5;
    float ceiling = pow(percipitationThingy, lerp(-15.0, 30.0, temperature)) / pow(percipitationThingy, 30.0);
    perp = perp * ceiling;
    return perp;
}

float3 get_biome(float percipitation, float temperature)
{
    
    float newPerp = get_percipitation_from_temperature(percipitation, temperature);
    
    float2 uv;
    
    uv.x = newPerp;
    uv.y = temperature;
    
    float3 result = biomeLUT.Sample(biomeSamp, uv);
    return result;
}

VSOutput VSMain(VSInput input)
{
    VSOutput output;

    float4 position = float4(input.position, 1.0);
    
    output.position = mul(position, transform);
    output.worldPos = mul(position, world).xyz;
    output.normal = normalize(mul(float4(input.normal, 0.0), normal));
    output.elevation = input.elevation;
    output.perp = input.perp;
    output.temp = input.temp;
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
    
    

    //return float4((objectColor.rgb * (ambient + diffuse) + specular) + (objectColor.rgb * pDiffuse + pSpecular) * pIntensity * pFalloff, 1.0);
    //return float4(get_color_from_elevation(input.elevation), 1.0); // Debug biomes
    //return float4(get_color_from_percipitation(input.perp), 1.0); // Debug percipitiation
    //return float4(get_color_from_temperature(input.temp), 1.0); // Debug temperature
    //return float4(get_color_from_percipitation(get_percipitation_from_temperature(input.perp, input.temp)), 1.0); // Debug percipitation based on temp
    return float4(get_biome(input.perp, input.temp), 1.0); // Debug biomes

}