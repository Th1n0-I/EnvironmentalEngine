Texture2D biomeLUT : register(t0);
SamplerState biomeSamp : register(s0);

Texture2D<uint> biomeIdLUT : register(t1);
SamplerState biomeIdSamp : register(s1);

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
    float elevationStrenght;
    float ElevationTemperatureScale;
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

float get_temp_from_elevation(float temp, float e)
{
     // Remap from 0, 1 to -1, 1 for underwater geometyr wowie I should do that in the generator eh problem for later also wow who new I could comment I never do that I probs should yeah let's start doing that it's probs the reason I'm always lost. Hello!
    float elevation = e;
    
    // ocean
    if (elevation <= 0)
        return -100;
    
    // make temperature dependent on height
    // Convert from 0,1 to C*
    float temperature = lerp(-10.0, 30.0, temp);
    
    // modify the temperature
    temperature -= elevation * ElevationTemperatureScale;
    
    // Covert back to 0,1
    temperature = (max(temperature, -10.0) + 10.0) / 40.0;
    
    return temperature;
}

float get_percipitation_from_temperature(float percipitation, float temperature, float e)
{
    float perp = percipitation * 0.5 + 0.5;
    float temp = get_temp_from_elevation(temperature, e);
    
    // Ocean
    if(temp == -100)
        return -100;
    
    float ceiling = pow(percipitationThingy, lerp(-10.0, 30.0, temp)) / pow(percipitationThingy, 30.0);
    perp = perp * ceiling;
    return perp;
}

float3 get_biome(float percipitation, float temperature, float e)
{
    
    float newPerp = get_percipitation_from_temperature(percipitation, temperature, e);
    
    //ocean
    if (newPerp == -100)
        return float3(0, 0, 0.5);
    
    float2 uv;
    
    uv.y = 1 - newPerp;
    uv.x = get_temp_from_elevation(temperature, e);
    
    float3 result = biomeLUT.Sample(biomeSamp, uv);
    return result;
}

int get_biome_id(float percipitation, float temperature, float e)
{
    float newPerp = get_percipitation_from_temperature(percipitation, temperature, e);
    
    //ocean
    if (newPerp == -100)
        return -1;
    
    float2 uv;
    uint w, h;
    biomeIdLUT.GetDimensions(w,h);
    
    uv.y = 1 - newPerp;
    uv.x = get_temp_from_elevation(temperature, e);
    uv *= float2(w, h);
    
    int result = biomeIdLUT.Load(int3((int2) uv, 0)).r;
    return result;
}

float3 get_biome_color_from_id(uint id)
{
    switch (id)
    {
        case -1:
            return float3(0.251, 0.769, 0.949);
            break;
        case 0:
            return float3(0.678, 0.882, 0.902);
            break;
        case 1:
            return float3(0.722, 0.776, 0.78);
            break;
        case 2:
            return float3(0.235, 0.529, 0.251);
            break;
        case 3:
            return float3(0.227, 0.612, 0.212);
            break;
        case 4:
            return float3(0.161, 0.302, 0.157);
            break;
        case 5:
            return float3(0.439, 0.871, 0.345);
            break;
        case 6:
            return float3(0.922, 0.678, 0.345);
            break;
        case 7:
            return float3(0.929, 0.906, 0.761);
            break;
        case 8:
            return float3(0.365, 0.471, 0.361);
            break;
        case 9:
            return float3(0.859, 0.847, 0.722);
            break;
        default: 
            return float3(1, 1, 1);
            break;

    }

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
    
    
    //objectColor.rgb = get_biome(input.perp, input.temp, input.elevation);
    objectColor.rgb = get_biome_color_from_id(get_biome_id(input.perp, input.temp, input.elevation));
    return float4((objectColor.rgb * (ambient + diffuse) + specular) + (objectColor.rgb * pDiffuse + pSpecular) * pIntensity * pFalloff, 1.0);
    //return float4(get_color_from_elevation(input.elevation), 1.0); // Debug biomes
    //return float4(get_color_from_percipitation(input.perp), 1.0); // Debug percipitiation
    //return float4(get_color_from_temperature(input.temp), 1.0); // Debug temperature
    //return float4(get_color_from_percipitation(get_percipitation_from_temperature(input.perp, input.temp)), 1.0); // Debug percipitation based on temp
    //return float4(get_biome(input.perp, input.temp, input.elevation), 1.0); // Debug biomes
    //return float4(get_biome_color_from_id(get_biome_id(input.perp, input.temp, input.elevation)), 1.0); // Biome with id so it doesnt blend and we can identify the biome for things like foliage

}