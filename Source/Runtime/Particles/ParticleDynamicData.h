#pragma once

#include "Math/EngineMath.h"
#include "Components/BillboardComponent.h" // FSubUVConstants (Billboard/ParticleSubUV 셰이더 cbuffer 레이아웃)

class FParticleSystemSceneProxy;
class FRenderQueue;
class UMaterial;
class UStaticMesh;
struct FViewContext;

/**
 * 이미터 동적 렌더 데이터 타입 열거형
 * (Unreal EDynamicEmitterType 대응)
 */
enum class EDynamicEmitterType : uint8
{
	DET_Unknown = 0,
	DET_Sprite,
	DET_Mesh,
	DET_Beam2,
	DET_Ribbon,
	DET_AnimTrail,
};

/**
 * 이미터 스냅샷의 공통 소스 데이터
 * (Unreal FDynamicEmitterReplayDataBase 대응)
 */
struct FDynamicEmitterReplayDataBase
{
	EDynamicEmitterType eEmitterType = EDynamicEmitterType::DET_Unknown;
	int32 ActiveParticleCount = 0;
	bool bUseLocalSpace = false;
};

/**
 * 스프라이트 파티클 1개의 GPU 인스턴스 데이터 (Unreal FParticleSpriteVertex 대응)
 * ParticleSpriteCommon.hlsli의 FParticleSpriteInstance와 바이트 단위로 같아야 한다.
 */
struct FParticleSpriteInstance
{
	float Position[3];      // 월드 위치
	float SubImageIndex;    // SubUV 칸 번호 (SubUV 모듈 전까지 0)
	float Size[2];          // 가로, 세로
	float Padding[2];
	FVector4 Color;         // rgb: Tint, a: 알파
};
static_assert(sizeof(FParticleSpriteInstance) == 48, "ParticleSpriteCommon.hlsli FParticleSpriteInstance와 크기가 같아야 한다");

/**
 * 메시 파티클 1개의 GPU 인스턴스 데이터 (Unreal FMeshParticleInstanceVertex 대응)
 * ParticleInstanceCommon.hlsli의 FParticleMeshInstance와 같아야 한다.
 */
struct FParticleMeshInstance
{
	FMatrix World;
};
static_assert(sizeof(FParticleMeshInstance) == 64, "ParticleInstanceCommon.hlsli FParticleMeshInstance와 크기가 같아야 한다");

/**
 * 렌더 스레드/SceneProxy가 다형성으로 소비하는 이미터별 동적 렌더 데이터 기반 클래스
 * (Unreal FDynamicEmitterDataBase 대응)
 */
struct FDynamicEmitterDataBase
{
	FDynamicEmitterDataBase() = default;
	virtual ~FDynamicEmitterDataBase() = default;

	virtual const FDynamicEmitterReplayDataBase& GetSource() const = 0;
	EDynamicEmitterType GetEmitterType() const { return GetSource().eEmitterType; }

	/** 각 이미터 타입이 자신의 렌더 패킷을 Queue에 직접 제출한다. */
	virtual void GetDynamicMeshElements(
		const FParticleSystemSceneProxy* Proxy,
		const FViewContext& View,
		FRenderQueue& Queue) const = 0;

	bool bSelected = false;
	int32 EmitterIndex = -1;
};

/**
 * Sprite 이미터용 스냅샷 데이터
 * (Unreal FDynamicSpriteEmitterReplayData 대응)
 */
struct FDynamicSpriteEmitterReplayData : public FDynamicEmitterReplayDataBase
{
	struct FSpriteParticleSnapshot
	{
		FVector Location = FVector::ZeroVector;
		FVector Size = FVector::OneVector;
		FVector4 Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	};

	TArray<FSpriteParticleSnapshot> Particles;
};

/**
 * 빌보드 Sprite 이미터 동적 렌더 데이터
 * (Unreal FDynamicSpriteEmitterData 대응)
 */
struct FDynamicSpriteEmitterData : public FDynamicEmitterDataBase
{
	FDynamicSpriteEmitterData()
	{
		Source.eEmitterType = EDynamicEmitterType::DET_Sprite;
	}

	virtual const FDynamicEmitterReplayDataBase& GetSource() const override { return Source; }
	virtual void GetDynamicMeshElements(
		const FParticleSystemSceneProxy* Proxy,
		const FViewContext& View,
		FRenderQueue& Queue) const override;

	/** 이미터 공통 머티리얼 파라미터(아틀라스 칸 수, sRGB 디코드)를 만든다. 스냅샷 생성 직후 1회 호출. */
	void BuildMaterialParams();

	FDynamicSpriteEmitterReplayData Source;
	UStaticMesh* QuadMesh = nullptr;
	UMaterial* Material = nullptr;

	/** 텍스처 아틀라스 가로/세로 칸 수. 지금은 0번 칸만 쓴다 (SubUV 애니메이션은 SubUV 모듈에서). */
	int32 SubImagesHorizontal = 1;
	int32 SubImagesVertical = 1;

	/**
	 * 이미터 공통 머티리얼 파라미터(b1). 렌더 패킷이 포인터로 참조하므로 이 스냅샷이 살아 있는 동안 유지된다.
	 * 파티클마다 다른 Tint/Alpha/프레임은 인스턴스 데이터(FParticleSpriteInstance)로 간다.
	 */
	FSubUVConstants EmitterParams{};
};

/**
 * Mesh 이미터용 스냅샷 데이터
 * (Unreal FDynamicMeshEmitterReplayData 대응)
 */
struct FDynamicMeshEmitterReplayData : public FDynamicEmitterReplayDataBase
{
	TArray<FMatrix> ParticleMatrices;
};

/**
 * StaticMesh 이미터 동적 렌더 데이터
 * (Unreal FDynamicMeshEmitterData 대응)
 */
struct FDynamicMeshEmitterData : public FDynamicEmitterDataBase
{
	FDynamicMeshEmitterData()
	{
		Source.eEmitterType = EDynamicEmitterType::DET_Mesh;
	}

	virtual const FDynamicEmitterReplayDataBase& GetSource() const override { return Source; }
	virtual void GetDynamicMeshElements(
		const FParticleSystemSceneProxy* Proxy,
		const FViewContext& View,
		FRenderQueue& Queue) const override;

	FDynamicMeshEmitterReplayData Source;
	UStaticMesh* StaticMesh = nullptr;
	UMaterial* OverrideMaterial = nullptr;
};

/**
 * 한 프레임의 UParticleSystemComponent 전체 이미터 동적 렌더 데이터 컨테이너
 * (Unreal FParticleDynamicData 대응)
 */
struct FParticleDynamicData
{
	TArray<TUniquePtr<FDynamicEmitterDataBase>> DynamicEmitterDataArray;
};
