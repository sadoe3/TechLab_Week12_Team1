#pragma pack_matrix(row_major)
#include "GpuSkinCommon.hlsli"
#include "ParticleInstanceCommon.hlsli"

// View별 행렬과 렌더 타깃 픽셀 크기·Outline 확장량을 전달한다.
// ViewportAndThickness.w: 0 = 일반 메시, 1 = 메시 파티클 인스턴스(t15), 2 = 스프라이트 파티클 인스턴스(t16)
cbuffer constants : register(b0)
{
    matrix World;
    matrix NormalMatrix;
    matrix ViewProjection;
    float4 ViewportAndThickness;
};

#if PARTICLE_INSTANCE
// ParticleDynamicData.h의 FParticleSpriteInstance와 같은 레이아웃 (48바이트).
// 메시 인스턴스(ParticleInstanceCommon.hlsli)가 t15를 쓰므로 Outline은 스프라이트 인스턴스를 t16에 따로 묶는다.
struct FOutlineSpriteInstance
{
    float3 Position;
    float SubImageIndex;
    float2 Size;
    float Rotation;     // 화면 안 회전(Roll), 라디안
    float Padding;
    float4 Color;
};
StructuredBuffer<FOutlineSpriteInstance> OutlineSpriteInstances : register(t16);
#endif

// 기존 위치·색상 Mesh 정점 입력 형식을 유지한다.
struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
    GPU_SKIN_VS_INPUT
    PARTICLE_INSTANCE_VS_INPUT
};


// 확장된 Clip 위치와 선택 Outline 색상을 전달한다.
struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

// 중심에서 바깥으로 향하는 투영 방향을 픽셀 공간에서 정규화해 일정 두께로 확장한다.
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    GPU_SKIN_APPLY(input, input.position, input.normal, input.tangent);

#if PARTICLE_INSTANCE
    if (ViewportAndThickness.w > 1.5f)
    {
        // 스프라이트: World에는 카메라 축(행 0 Facing, 행 1 Right, 행 2 Up)이 들어 있다 (FDynamicSpriteEmitterData와 같은 규칙).
        // 쿼드 중심에서 꼭짓점 쪽을 법선 대신 써서, 화면에서 쿼드 바깥으로 밀어낸다.
        const FOutlineSpriteInstance Particle = OutlineSpriteInstances[input.ParticleInstanceID];
        // ParticleSpriteRotatedOffset(ParticleSpriteCommon.hlsli)와 같은 회전: 크기를 곱한 뒤 Roll만큼 돌린다
        const float2 Scaled = float2(input.position.y * Particle.Size.x, input.position.z * Particle.Size.y);
        float S, C;
        sincos(Particle.Rotation, S, C);
        const float3 Offset = World[1].xyz * (Scaled.x * C - Scaled.y * S)
                            + World[2].xyz * (Scaled.x * S + Scaled.y * C);
        input.position = Particle.Position + World[0].xyz * input.position.x + Offset;
        input.normal = Offset;
    }
    else
    {
        // 메시 파티클: 인스턴스 월드 행렬로 위치·법선을 월드로 옮긴다. World/NormalMatrix는 단위 행렬로 들어온다.
        PARTICLE_INSTANCE_APPLY(input, input.position, input.normal, input.tangent);
    }
    const float4 worldPosition = float4(input.position, 1.0f);
    const float4 worldNormal = float4(input.normal, 0.0f);
#else
    const float4 worldPosition = mul(float4(input.position, 1.0f), World);
    const float4 worldNormal = mul(float4(input.normal, 0.0f), NormalMatrix);
#endif

    float4 clipPosition = mul(worldPosition, ViewProjection);
    const float4 clipDirection = mul(worldNormal, ViewProjection);
    const float2 viewportSize = max(ViewportAndThickness.xy, float2(1.0f, 1.0f));

    // 원근 나눗셈의 방향 미분을 사용하고 W 보정으로 원근·직교 모두 픽셀 폭을 유지한다.
    float2 pixelDirection = (clipDirection.xy * clipPosition.w - clipPosition.xy * clipDirection.w) * viewportSize;

    const float lengthSquared = dot(pixelDirection, pixelDirection);

    if (clipPosition.w > 1.0e-6f && lengthSquared > 1.0e-12f)
    {
        pixelDirection *= rsqrt(lengthSquared);
        clipPosition.xy += pixelDirection * (2.0f * ViewportAndThickness.z / viewportSize) * clipPosition.w;
    }
    output.position = clipPosition;
    output.color = float4(1.0f, 1.0f, 0.0f, 1.0f);
    return output;
}

// 선택 Outline의 노란색을 출력한다.
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    return input.color;
}
