
cbuffer AtmosphereConstants : register(b0)
{
    float4x4 invViewProj;
    float3 campos;
    float innerRadius;
    float3 planetCenter;
    float outerRadius;
    float3 dirToSun;
    float scaleHeight;
    float3 rayleighCoeff;
    float sunIntensity;
};

Texture2D depthTex : register(t0);

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput VSMain(uint id : SV_VertexID){
    VSOutput o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.uv = uv;
    o.position = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}

float2 raySphere(float3 center, float radius, float3 origin, float3 rayDir)
{
    float3 oc = origin - center;
    float b = dot(oc, rayDir);
    float c = dot(oc, oc) - radius * radius;
    float disc = b * b - c;
    if (disc < 0.0)
        return float2(1, -1);
    float s = sqrt(disc);
    return float2(-b - s, -b + s);

}

static const float PI = 3.14159265;

float4 PSMain(VSOutput input) : SV_Target
{
    
    float depth = depthTex.Load(int3(input.position.xy, 0)).r;
    
    float2 ndc = float2(input.uv.x * 2.0 - 1.0, 1.0 - input.uv.y * 2.0);
    
    float4 clip = float4(ndc, depth, 1.0);
    float4 world = mul(clip, invViewProj);
    world /= world.w;
    
    float3 worldPos = world.xyz;
    
    float sceneDist = length(worldPos - campos);
    
    float3 rayDir = normalize(worldPos - campos); 
    
    float2 hit = raySphere(planetCenter, outerRadius, campos, rayDir);
    hit.x = max(hit.x, 0.0);
    
    if (hit.y < 0.0)
        return float4(0, 0, 0, 0);
    
    float tNear = max(hit.x, 0.0f);
    float tFar = min(hit.y, sceneDist);
    if (tFar <= tNear)
        return float4(0, 0, 0, 0);
    
    const int STEPS = 16, LIGHT_STEPS = 16;
    float stepSize = (tFar - tNear) / STEPS;
    float viewOD = 0.0;
    float3 inScatter = 0.0;
    
    for (int i = 0; i < STEPS; i++)
    {
        float3 p = campos + rayDir * (tNear + (i + 0.5) * stepSize);
        float h = length(p - planetCenter) - innerRadius;
        float d = exp(-h / scaleHeight) * stepSize;
        viewOD += d;
        
        float lightFar = raySphere(planetCenter, outerRadius, p, dirToSun).y;
        float lStep = lightFar / LIGHT_STEPS;
        float sunOD = 0.0;
        for (int j = 0; j < LIGHT_STEPS; j++)
        {
            float3 lp = p + dirToSun * ((j + 0.5) * lStep);
            float lh = length(lp - planetCenter) - innerRadius;
            sunOD += exp(-lh / scaleHeight) * lStep;
        }

        float3 transmittance = exp(-rayleighCoeff * (viewOD + sunOD));
        inScatter += d * transmittance;
    }
    
    float cosT = dot(rayDir, dirToSun);
    float phase = 3.0 / (16.0 * PI) * (1.0 + cosT * cosT);
    inScatter *= rayleighCoeff * phase * sunIntensity;
    
    return float4(inScatter, 1.0);
}