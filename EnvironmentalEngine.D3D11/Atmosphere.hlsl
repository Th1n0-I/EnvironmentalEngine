
cbuffer AtmosphereConstants : register(b0)
{
    float4x4 invViewProj;
    float3 campos;
    float padding0;
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
    
    
    
    return float4(1 - sceneDist.xxx * 0.2, 1);

}