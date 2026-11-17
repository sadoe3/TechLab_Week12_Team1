#pragma pack_matrix(row_major)
#include "ColorSpace.hlsli"
#include "ParticleSpriteCommon.hlsli"
// 스프라이트 파티클 셰이더 (Particle_AlphaBlend / Particle_Additive PSO)
// 파티클 시스템은 "#PARTICLE_INSTANCE" 변형으로 이미터당 한 번 그린다. 기본 변형은 b1 값으로 쿼드 하나를 그린다.

cbuffer constants : register(b0)
{
    matrix VP;
};

// 인스턴스 변형: 카메라 축(Facing, Right, Up). 기본 변형: 쿼드의 월드 행렬.
cbuffer Worldconstants : register(b2)
{
    matrix World;
};

// BillboardComponent.h의 FSubUVConstants와 바이트 단위로 같아야 한다.
// 인스턴스 변형에서는 이미터 공통 값(AtlasRowSize, AtlasColSize, DecodeSRGB)만 쓰고 프레임·알파·Tint는 인스턴스 데이터에서 읽는다.
cbuffer subuv : register(b1)
{
    float CurrentFrame;
    float AtlasRowSize;
    float AtlasColSize;
    float ParticleAlpha;
    float4 Tint;
    // 1: HDR 출력일 때 sRGB -> 선형 변환, 0: 현재 LDR 반투명 패스에서는 그대로 사용
    float DecodeSRGB;
    float3 SubUVPadding;
};

Texture2D AtlasTexture : register(t0);
SamplerState AtlasSampler : register(s0);

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
    nointerpolation float frame : TEXCOORD1;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
#if PARTICLE_INSTANCE
    const FParticleSpriteInstance Particle = ParticleSpriteInstances[input.ParticleInstanceID];
    const float3 WorldPosition = ParticleSpriteWorldPosition(input.position, Particle, World);
    output.position = mul(float4(WorldPosition, 1.0f), VP);
    output.color = Particle.Color;
    output.frame = Particle.SubImageIndex;
#else
    output.position = mul(mul(float4(input.position, 1.0f), World), VP);
    output.color = float4(Tint.rgb, ParticleAlpha);
    output.frame = CurrentFrame;
#endif
    output.uv = input.uv;
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float atlasCol = fmod(input.frame, AtlasColSize);
    float atlasRow = floor(input.frame / AtlasColSize);

    float2 cellSize = float2(1.0f / AtlasColSize, 1.0f / AtlasRowSize);
    float2 cellUV = input.uv * cellSize;

    float2 cellStartOffsetUV = (float2(atlasCol, atlasRow) * cellSize);
    cellUV += cellStartOffsetUV;

    float4 color = AtlasTexture.Sample(AtlasSampler, cellUV);
    color.a *= input.color.a;
    if (DecodeSRGB > 0.5f)
    {
        // 텍스처와 Tint를 각각 선형으로 바꾼 뒤 곱한다. 알파는 색이 아니므로 그대로.
        color.rgb = SRGBToLinear(color.rgb) * SRGBToLinear(input.color.rgb);
    }
    else
    {
        color.rgb *= input.color.rgb;
    }

    return color;
}
