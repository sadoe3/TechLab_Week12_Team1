#pragma once

#include "Math/EngineMath.h"
#include "Components/BillboardComponent.h" // FSubUVConstants (Billboard/ParticleSubUV 셰이더 cbuffer 레이아웃)
#include "Render/Buffer.h"

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
	/** RequiredModule->SortMode (EParticleSortMode 값) */
	uint8 SortMode = 0;
};

/**
 * 스프라이트 파티클 1개의 GPU 인스턴스 데이터 (Unreal FParticleSpriteVertex 대응)
 * ParticleSpriteCommon.hlsli의 FParticleSpriteInstance와 바이트 단위로 같아야 한다.
 */
struct FParticleSpriteInstance
{
	float Position[3];      // 월드 위치
	float SubImageIndex;    // SubUV 칸 번호 (UParticleModuleSubUV가 없으면 0)
	float Size[2];          // 가로, 세로
	float Rotation;         // 화면 안 회전(Roll), 라디안
	float Padding;
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
	FVector4 Color;         // rgb: Tint, a: 알파 (Color/ColorOverLife 모듈 결과)
};
static_assert(sizeof(FParticleMeshInstance) == 80, "ParticleInstanceCommon.hlsli FParticleMeshInstance와 크기가 같아야 한다");

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
		/** 수명 진행도 0~1 (Age 정렬용) */
		float RelativeTime = 0.0f;
		/** 화면 안 회전(FBaseParticle::Rotation.Z = Roll), 도 */
		float Rotation = 0.0f;
		/** SubUV 아틀라스 칸 번호 (UParticleModuleSubUV Payload) */
		float SubImageIndex = 0.0f;
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

	/** 텍스처 아틀라스 가로/세로 칸 수. 파티클마다 어느 칸을 그릴지는 UParticleModuleSubUV가 정한다. */
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
	/** ParticleMatrices와 같은 순서의 수명 진행도 0~1 (Age 정렬용) */
	TArray<float> RelativeTimes;
	/** ParticleMatrices와 같은 순서의 파티클 색 (FBaseParticle::Color) */
	TArray<FVector4> Colors;
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
	/** UParticleModuleTypeDataMesh::bCastShadows. 컴포넌트 bCastShadow도 켜져 있어야 그림자 패스에 들어온다. */
	bool bCastShadows = false;
};

/**
 * 리본 띠의 정점 1개
 * RibbonShader.hlsl의 VS_INPUT과 필드 순서·크기가 같아야 한다 
 */
struct FParticleRibbonVertex
{
	FVector Position;   // 월드 위치 (CPU에서 이미 좌우로 벌려 둔 값)
	FVector2 UV;        // U: 띠 길이 방향, V: 0(왼쪽)~1(오른쪽)
	FVector4 Color;
};
static_assert(sizeof(FParticleRibbonVertex) == 36, "RibbonShader.hlsl VS_INPUT과 크기가 같아야 한다");

/**
 * 리본 이미터 인스턴스가 소유하는 동적 VB/IB. 스냅샷은 매 프레임 새로 만들어지므로 GPU 버퍼는 여기서 재사용한다.
 * 인스턴스와 스냅샷이 함께 들고 있어서, 인스턴스가 먼저 사라져도 프록시의 마지막 스냅샷이 안전하다.
 */
struct FRibbonRenderBuffers
{
	TUniquePtr<FVertexBuffer> VertexBuffer;
	TUniquePtr<FIndexBuffer> IndexBuffer;
	uint32 VertexCapacity = 0;
	uint32 IndexCapacity = 0;
};

/**
 * Ribbon 이미터용 스냅샷 데이터
 */
struct FDynamicRibbonEmitterReplayData : public FDynamicEmitterReplayDataBase
{
	struct FRibbonPoint
	{
		FVector Location = FVector::ZeroVector;
		/** 띠 전체 폭 (Particle.Size.X) */
		float Width = 1.0f;
		FVector4 Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	};

	/** 머리(가장 최근에 태어난 점) → 꼬리(가장 오래된 점) 순서 */
	TArray<FRibbonPoint> Points;
};

/**
 * 점 목록을 카메라를 향한 띠로 그리는 Ribbon 이미터 동적 렌더 데이터
 */
struct FDynamicRibbonEmitterData : public FDynamicEmitterDataBase
{
	FDynamicRibbonEmitterData()
	{
		Source.eEmitterType = EDynamicEmitterType::DET_Ribbon;
	}

	virtual const FDynamicEmitterReplayDataBase& GetSource() const override { return Source; }
	virtual void GetDynamicMeshElements(
		const FParticleSystemSceneProxy* Proxy,
		const FViewContext& View,
		FRenderQueue& Queue) const override;

	/**
	 * 월드 공간 점 목록 → 점마다 정점 2개(2i = 왼쪽 V=0, 2i+1 = 오른쪽 V=1).
	 * 띠는 카메라를 향해 펼쳐지고, 폭은 Width, 색은 Color를 쓴다.
	 */
	static void BuildRibbonVertices(const TArray<FDynamicRibbonEmitterReplayData::FRibbonPoint>& WorldPoints,
		const FViewContext& View, TArray<FParticleRibbonVertex>& OutVertices);

	FDynamicRibbonEmitterReplayData Source;
	UMaterial* Material = nullptr;
	TSharedPtr<FRibbonRenderBuffers> Buffers;
};

/**
 * 한 프레임의 UParticleSystemComponent 전체 이미터 동적 렌더 데이터 컨테이너
 * (Unreal FParticleDynamicData 대응)
 */
struct FParticleDynamicData
{
	TArray<TUniquePtr<FDynamicEmitterDataBase>> DynamicEmitterDataArray;
};
