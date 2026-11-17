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
    float2 Padding;
    float4 Color;           // rgb: Tint, a: 알파
};

// FRenderer::BindInstances가 배치마다 채운다 (ParticleInstanceBufferSlot)
StructuredBuffer<FParticleSpriteInstance> ParticleSpriteInstances : register(t15);

#define PARTICLE_INSTANCE_VS_INPUT \
    uint ParticleInstanceID : SV_InstanceID;

// CameraAxes는 FDynamicSpriteEmitterData가 오브젝트 상수(b2 World)에 실어 보낸 카메라 축이다.
// 행 0: 카메라를 보는 방향(Facing), 행 1: 화면 오른쪽(Right), 행 2: 화면 위쪽(Up). 이동 성분은 0.
// FViewContext::BuildBillboardMatrix(Position, Size.x, Size.y)와 같은 행렬을 만들어 곱한 것과 결과가 같다.
float3 ParticleSpriteWorldPosition(float3 LocalPosition, FParticleSpriteInstance Particle, float4x4 CameraAxes)
{
    return Particle.Position
        + CameraAxes[0].xyz * LocalPosition.x
        + CameraAxes[1].xyz * (LocalPosition.y * Particle.Size.x)
        + CameraAxes[2].xyz * (LocalPosition.z * Particle.Size.y);
}

#else

#define PARTICLE_INSTANCE_VS_INPUT

#endif

#endif
