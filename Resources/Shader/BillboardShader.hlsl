#pragma pack_matrix(row_major)
#include "ParticleSpriteCommon.hlsli"
// 기본 변형: UBillboardComponent 아이콘 1개. "#PARTICLE_INSTANCE" 변형: 머티리얼이 없는 스프라이트 이미터(BillboardMaterial)를 인스턴싱으로 그린다.

cbuffer constants : register(b0)
{
    matrix VP;
};

// 기본 변형: 빌보드 월드 행렬. 인스턴스 변형: 카메라 축(Facing, Right, Up).
cbuffer Worldconstants : register(b2)
{
    matrix World;
};

// UBillboardComponent가 제출하는 FSubUVConstants와 동일한 레이아웃이다.
cbuffer MaterialParams : register(b1)
{
    float CurrentFrame;
    float AtlasRowSize;
    float AtlasColSize;
    float SpriteAlpha;
    float4 Tint;
    float DecodeSRGB;
    float3 Padding;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    PARTICLE_INSTANCE_VS_INPUT
};

// 두 변형이 PS를 함께 쓰므로 PS 입력은 같아야 한다.
struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

Texture2D SpriteTexture : register(t0);
SamplerState SpriteSampler : register(s0);

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
#if PARTICLE_INSTANCE
    const FParticleSpriteInstance Particle = ParticleSpriteInstances[input.ParticleInstanceID];
    const float4 WorldPosition = float4(ParticleSpriteWorldPosition(input.position, Particle, World), 1.0f);
    output.color = Particle.Color;
#else
    const float4 WorldPosition = mul(float4(input.position, 1.0f), World);
    output.color = float4(Tint.rgb, SpriteAlpha);
#endif
    output.position = mul(WorldPosition, VP);
    output.uv = input.uv;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 color = SpriteTexture.Sample(SpriteSampler, input.uv);
    color.rgb *= input.color.rgb;
    color.a *= input.color.a;
    return color;
}
