// Resources/Shader/ParticleInstanceCommon.hlsli
// 메시 파티클 인스턴싱. 메시를 그리는 패스의 VS가 공유한다. PARTICLE_INSTANCE=1로 컴파일한 변형에서만 동작한다.
// (Unreal FMeshParticleVertexFactory 대응: 이미터 하나를 DrawIndexedInstanced 한 번으로 그리고, 인스턴스마다 다른 월드 행렬을 쓴다)
#ifndef PARTICLE_INSTANCE_COMMON_HLSLI
#define PARTICLE_INSTANCE_COMMON_HLSLI

#ifndef PARTICLE_INSTANCE
#define PARTICLE_INSTANCE 0
#endif

#if PARTICLE_INSTANCE

// ParticleDynamicData.h의 FParticleMeshInstance와 같은 레이아웃
struct FParticleMeshInstance
{
    row_major float4x4 World;
    float4 Color; // rgb: Tint, a: 알파 (파티클 Color 모듈 결과)
};

// FRenderer::BindInstances가 배치마다 채운다 (ParticleInstanceBufferSlot)
StructuredBuffer<FParticleMeshInstance> ParticleMeshInstances : register(t15);

// 입력 레이아웃은 리플렉션으로 만들어지며 SV_ 시스템 값은 건너뛰므로 정점 버퍼 레이아웃은 바뀌지 않는다
#define PARTICLE_INSTANCE_VS_INPUT \
    uint ParticleInstanceID : SV_InstanceID;

float3 ParticleInstanceSafeNormalize(float3 V)
{
    const float LengthSquared = dot(V, V);
    return LengthSquared > 1.0e-12f ? V / sqrt(LengthSquared) : V;
}

// 비균등 스케일에서도 면에 수직이 되도록 3x3 부분의 여인수 행렬로 노멀을 옮긴다 (GpuSkinTransformNormal과 같은 방식)
float3 ParticleInstanceTransformNormal(float4x4 M, float3 N)
{
    const float3 R0 = M[0].xyz;
    const float3 R1 = M[1].xyz;
    const float3 R2 = M[2].xyz;
    const float3 C0 = cross(R1, R2);
    const float3 Result = C0 * N.x + cross(R2, R0) * N.y + cross(R0, R1) * N.z;
    return ParticleInstanceSafeNormalize(dot(R0, C0) < 0.0f ? -Result : Result);
}

// 정점을 인스턴스 월드 공간으로 옮긴다. 이후 코드는 오브젝트 상수의 World(= 단위 행렬, 그림자는 PreTranslation만)를 그대로 곱한다.
void ParticleInstanceVertex(uint InstanceID, inout float3 Position, inout float3 Normal)
{
    const float4x4 M = ParticleMeshInstances[InstanceID].World;
    Position = mul(float4(Position, 1.0f), M).xyz;
    Normal = ParticleInstanceTransformNormal(M, Normal);
}

void ParticleInstanceTangent(uint InstanceID, inout float4 Tangent)
{
    const float4x4 M = ParticleMeshInstances[InstanceID].World;
    Tangent.xyz = ParticleInstanceSafeNormalize(mul(float4(Tangent.xyz, 0.0f), M).xyz);
    // 거울 반전(행렬식 음수) 인스턴스면 바이탄젠트 방향도 뒤집는다
    Tangent.w *= determinant((float3x3)M) < 0.0f ? -1.0f : 1.0f;
}

#define PARTICLE_INSTANCE_APPLY(Input, Position, Normal, Tangent) \
    ParticleInstanceVertex(Input.ParticleInstanceID, Position, Normal); \
    ParticleInstanceTangent(Input.ParticleInstanceID, Tangent)
// 탄젠트를 입력으로 받지 않는 셰이더(그림자 깊이)용
#define PARTICLE_INSTANCE_APPLY_PN(Input, Position, Normal) \
    ParticleInstanceVertex(Input.ParticleInstanceID, Position, Normal)
// 인스턴스 색. VS가 PS로 넘겨 표면 색에 곱한다.
#define PARTICLE_INSTANCE_COLOR(Input) ParticleMeshInstances[Input.ParticleInstanceID].Color

#else

#define PARTICLE_INSTANCE_VS_INPUT
#define PARTICLE_INSTANCE_APPLY(Input, Position, Normal, Tangent)
#define PARTICLE_INSTANCE_APPLY_PN(Input, Position, Normal)
// PS는 인스턴스/일반 VS 변형이 함께 쓰므로 VS 출력 구조는 같아야 한다. 일반 메시는 흰색(곱셈 항등)을 넘긴다.
#define PARTICLE_INSTANCE_COLOR(Input) float4(1.0f, 1.0f, 1.0f, 1.0f)

#endif

#endif
