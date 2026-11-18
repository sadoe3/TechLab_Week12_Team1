#pragma pack_matrix(row_major)
// 리본 띠: CPU가 이미 카메라를 향해 좌우로 벌린 월드 정점을 그대로 그린다 (FParticleRibbonVertex)

cbuffer ViewConstants : register(b0)
{
    matrix VP;
};

// 정점이 이미 월드 공간이라 단위 행렬이 들어온다
cbuffer WorldConstants : register(b2)
{
    matrix World;
};

// 입력 레이아웃은 리플렉션으로 만들어지므로 FParticleRibbonVertex와 순서가 같아야 한다
struct VS_INPUT
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

Texture2D RibbonTexture : register(t0);
SamplerState RibbonSampler : register(s0);

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    const float4 WorldPosition = mul(float4(input.position, 1.0f), World);
    output.position = mul(WorldPosition, VP);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    return RibbonTexture.Sample(RibbonSampler, input.uv) * input.color;
}
