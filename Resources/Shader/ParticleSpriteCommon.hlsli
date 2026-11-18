// Resources/Shader/ParticleSpriteCommon.hlsli
// 스프라이트 파티클 인스턴싱. PARTICLE_INSTANCE=1 변형에서 쿼드 하나를 파티클 수만큼 인스턴스로 그린다.
// (Unreal FParticleSpriteVertexFactory 대응: 파티클마다 FParticleSpriteVertex 하나를 인스턴스 데이터로 읽는다)
#ifndef PARTICLE_SPRITE_COMMON_HLSLI
#define PARTICLE_SPRITE_COMMON_HLSLI

#ifndef PARTICLE_INSTANCE
#define PARTICLE_INSTANCE 0
#endif

#if PARTICLE_INSTANCE

// ParticleDynamicData.h의 FParticleSpriteInstance와 같은 레이아웃 (48바이트)
struct FParticleSpriteInstance
{
    float3 Position;        // 월드 위치 (로컬 공간 이미터도 CPU에서 월드로 옮겨 둔다)
    float SubImageIndex;    // SubUV 아틀라스 칸 번호
    float2 Size;            // 가로, 세로
    float Rotation;         // 화면 안 회전(Roll), 라디안
    float Padding;
    float4 Color;           // rgb: Tint, a: 알파
};

// FRenderer::BindInstances가 배치마다 채운다 (ParticleInstanceBufferSlot)
StructuredBuffer<FParticleSpriteInstance> ParticleSpriteInstances : register(t15);

#define PARTICLE_INSTANCE_VS_INPUT \
    uint ParticleInstanceID : SV_InstanceID;

// CameraAxes는 FDynamicSpriteEmitterData가 오브젝트 상수(b2 World)에 실어 보낸 카메라 축이다.
// 행 0: 카메라를 보는 방향(Facing), 행 1: 화면 오른쪽(Right), 행 2: 화면 위쪽(Up). 이동 성분은 0.
// Rotation이 0이면 FViewContext::BuildBillboardMatrix(Position, Size.x, Size.y)와 같은 행렬을 만들어 곱한 것과 결과가 같다.

// 쿼드 꼭짓점의 화면 평면 오프셋(Right, Up 성분). 먼저 크기를 곱한 뒤 Roll만큼 돌려서
// 직사각형 스프라이트도 모양을 유지한 채 회전한다. 양수 Rotation = 화면에서 반시계 방향.
float2 ParticleSpriteRotatedOffset(float3 LocalPosition, float2 Size, float Rotation)
{
    const float2 Scaled = float2(LocalPosition.y * Size.x, LocalPosition.z * Size.y);
    float S, C;
    sincos(Rotation, S, C);
    return float2(Scaled.x * C - Scaled.y * S, Scaled.x * S + Scaled.y * C);
}

float3 ParticleSpriteWorldPosition(float3 LocalPosition, FParticleSpriteInstance Particle, float4x4 CameraAxes)
{
    const float2 Offset = ParticleSpriteRotatedOffset(LocalPosition, Particle.Size, Particle.Rotation);
    return Particle.Position
        + CameraAxes[0].xyz * LocalPosition.x
        + CameraAxes[1].xyz * Offset.x
        + CameraAxes[2].xyz * Offset.y;
}

#else

#define PARTICLE_INSTANCE_VS_INPUT

#endif

#endif
