#include "EnginePCH.h"
#include "Particles/ParticleDynamicData.h"
#include "Particles/ParticleModuleRequired.h"
#include "Engine/ParticleSystemSceneProxy.h"
#include "Render/Mesh.h"
#include "Render/RenderPacket.h"
#include "Render/RenderResourceManager.h"
#include <algorithm>

namespace
{
	/**
	 * 이미터 안의 파티클 그리기 순서를 정한다 (Unreal FDynamicSpriteEmitterDataBase::SortSpriteParticles 대응).
	 * OutOrder[i] = i번째로 그릴 파티클의 인덱스. 뒤에서 앞으로 그려야 알파 블렌딩이 맞으므로 거리·깊이는 큰 것부터.
	 * 카메라 정보가 없는 뷰는 정렬하지 않는다.
	 */
	template <typename FGetPosition, typename FGetRelativeTime>
	void BuildSortOrder(int32 Count, uint8 InSortMode, const FViewContext& View,
		FGetPosition&& GetPosition, FGetRelativeTime&& GetRelativeTime, TArray<int32>& OutOrder)
	{
		OutOrder.SetNum(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			OutOrder[Index] = Index;
		}

		const EParticleSortMode SortMode = static_cast<EParticleSortMode>(InSortMode);
		const bool bHasView = View.CameraForward.Dot(View.CameraForward) > 1e-12f;
		if (SortMode == EParticleSortMode::PSORTMODE_None || Count < 2)
		{
			return;
		}
		if (!bHasView && (SortMode == EParticleSortMode::PSORTMODE_ViewProjDepth || SortMode == EParticleSortMode::PSORTMODE_DistanceToView))
		{
			return;
		}

		// 키를 한 번만 계산하고, 키가 큰 것부터 그린다
		TArray<float> Keys;
		Keys.SetNum(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			float Key = 0.0f;
			switch (SortMode)
			{
			case EParticleSortMode::PSORTMODE_ViewProjDepth:
				Key = (GetPosition(Index) - View.CameraPosition).Dot(View.CameraForward);
				break;
			case EParticleSortMode::PSORTMODE_DistanceToView:
			{
				const FVector Delta = GetPosition(Index) - View.CameraPosition;
				Key = Delta.Dot(Delta);
				break;
			}
			case EParticleSortMode::PSORTMODE_Age_OldestFirst:
				Key = GetRelativeTime(Index);
				break;
			case EParticleSortMode::PSORTMODE_Age_NewestFirst:
				Key = -GetRelativeTime(Index);
				break;
			default:
				break;
			}
			Keys[Index] = Key;
		}

		std::stable_sort(OutOrder.begin(), OutOrder.end(), [&Keys](int32 A, int32 B)
		{
			return Keys[A] > Keys[B];
		});
	}

	FVector TransformPosition(const FVector& Position, const FMatrix& Matrix)
	{
		const FVector4 Transformed = FVector4(Position.X, Position.Y, Position.Z, 1.0f) * Matrix;
		return FVector(Transformed.X, Transformed.Y, Transformed.Z);
	}

	/** 이미터 배치 하나를 반투명 큐에서 정렬할 거리: 카메라에서 파티클 중심까지 거리의 제곱 */
	float DistanceSqToCenter(const FVector& Sum, int32 Count, const FViewContext& View)
	{
		if (Count <= 0)
		{
			return 0.0f;
		}
		const float Inv = 1.0f / static_cast<float>(Count);
		const FVector Center(Sum.X * Inv, Sum.Y * Inv, Sum.Z * Inv);
		const FVector Delta = Center - View.CameraPosition;
		return Delta.Dot(Delta);
	}
}

void FDynamicSpriteEmitterData::BuildMaterialParams()
{
	// CurrentFrame, AtlasRowSize(세로 칸 수), AtlasColSize(가로 칸 수), Alpha
	// 인스턴스 셰이더는 프레임·알파·Tint를 인스턴스 데이터에서 읽고, 여기서는 이미터 공통 값만 쓴다.
	EmitterParams = FSubUVConstants{ 0.0f,
		static_cast<float>(std::max(SubImagesVertical, 1)),
		static_cast<float>(std::max(SubImagesHorizontal, 1)),
		1.0f };
	EmitterParams.Tint = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	// 반투명 패스는 톤 매핑 이후 LDR SceneColor에 그려지므로 Billboard와 동일하게 sRGB 디코드를 끈다.
	EmitterParams.DecodeSRGB = 0.0f;
}

void FDynamicSpriteEmitterData::GetDynamicMeshElements(
	const FParticleSystemSceneProxy* Proxy,
	const FViewContext& View,
	FRenderQueue& Queue) const
{
	if (!Proxy || !QuadMesh || !Material || Source.Particles.IsEmpty())
	{
		return;
	}

	FVertexBuffer* VB = QuadMesh->GetVertexBuffer(0);
	FIndexBuffer* IB = QuadMesh->GetIndexBuffer(0);
	if (!VB || !IB)
	{
		return;
	}

	const FMatrix& ComponentToWorld = Proxy->GetLocalToWorld();
	const FStaticMeshData& MeshData = QuadMesh->GetMeshData(0);
	const int32 Count = Source.Particles.Num();

	const auto WorldPositionOf = [&](int32 Index)
	{
		const FVector& Location = Source.Particles[Index].Location;
		return Source.bUseLocalSpace ? TransformPosition(Location, ComponentToWorld) : Location;
	};

	// 1) 이미터 내부 정렬 (SortMode)
	TArray<int32> Order;
	BuildSortOrder(Count, Source.SortMode, View, WorldPositionOf,
		[&](int32 Index) { return Source.Particles[Index].RelativeTime; }, Order);

	// 2) 정렬 순서대로 인스턴스 데이터 채우기. 큐가 이번 프레임 동안 보관한다.
	FParticleSpriteInstance* Instances = Queue.AllocateInstances<FParticleSpriteInstance>(static_cast<uint32>(Count));
	FVector PositionSum = FVector::ZeroVector;
	for (int32 DrawIndex = 0; DrawIndex < Count; ++DrawIndex)
	{
		const FDynamicSpriteEmitterReplayData::FSpriteParticleSnapshot& Particle = Source.Particles[Order[DrawIndex]];
		const FVector WorldPos = WorldPositionOf(Order[DrawIndex]);
		PositionSum = PositionSum + WorldPos;

		FParticleSpriteInstance& Instance = Instances[DrawIndex];
		Instance.Position[0] = WorldPos.X;
		Instance.Position[1] = WorldPos.Y;
		Instance.Position[2] = WorldPos.Z;
		Instance.SubImageIndex = 0.0f; // SubUV 애니메이션 전까지 아틀라스 0번 칸
		Instance.Size[0] = Particle.Size.X;
		Instance.Size[1] = Particle.Size.Y;
		Instance.Padding[0] = Instance.Padding[1] = 0.0f;
		Instance.Color = Particle.Color;
	}

	// 3) 카메라 축 행렬(Facing, Right, Up, 이동 0)을 오브젝트 상수(World)로 보낸다. VS가 파티클마다 빌보드를 세운다.
	const FMatrix* CameraAxes = Queue.StoreWorldMatrix(View.BuildBillboardMatrix(FVector::ZeroVector, 1.0f, 1.0f));
	const float EmitterDistSq = DistanceSqToCenter(PositionSum, Count, View);

	// 4) 이미터당 패킷 1개 (쿼드 섹션이 여럿이면 섹션당 1개) = 드로우콜 1번
	const auto AddPacket = [&](uint32 StartIndex, uint32 IndexCount)
	{
		FRenderPacket& Packet = Queue.AddDefaulted_GetRef();
		Packet.Proxy = Proxy;
		Packet.Model = CameraAxes;
		Packet.Mesh = QuadMesh;
		Packet.VertexBuffer = VB;
		Packet.IndexBuffer = IB;
		Packet.Material = Material;
		Packet.MaterialParamData = &EmitterParams;
		Packet.MaterialParamDataSize = sizeof(FSubUVConstants);
		Packet.CameraToParticleDistance = EmitterDistSq;
		Packet.StartIndex = StartIndex;
		Packet.IndexCount = IndexCount;
		Packet.InstanceData = Instances;
		Packet.InstanceStride = sizeof(FParticleSpriteInstance);
		Packet.InstanceCount = static_cast<uint32>(Count);
	};

	if (MeshData.Sections.IsEmpty())
	{
		AddPacket(0, 0);
	}
	else
	{
		for (const FStaticMeshSection& Section : MeshData.Sections)
		{
			AddPacket(Section.StartIndex, Section.IndexCount);
		}
	}
}

void FDynamicMeshEmitterData::GetDynamicMeshElements(
	const FParticleSystemSceneProxy* Proxy,
	const FViewContext& View,
	FRenderQueue& Queue) const
{
	if (!Proxy || !StaticMesh || Source.ParticleMatrices.IsEmpty())
	{
		return;
	}

	FVertexBuffer* VB = StaticMesh->GetVertexBuffer(0);
	FIndexBuffer* IB = StaticMesh->GetIndexBuffer(0);
	if (!VB || !IB)
	{
		return;
	}

	const FMatrix& ComponentToWorld = Proxy->GetLocalToWorld();
	const FStaticMeshData& MeshData = StaticMesh->GetMeshData(0);
	const int32 Count = Source.ParticleMatrices.Num();

	// 1) 월드 행렬 계산 (로컬 공간 이미터는 컴포넌트 변환을 곱한다)
	TArray<FMatrix> WorldMatrices;
	WorldMatrices.SetNum(Count);
	FVector PositionSum = FVector::ZeroVector;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FMatrix& LocalOrWorld = Source.ParticleMatrices[Index];
		WorldMatrices[Index] = Source.bUseLocalSpace ? (LocalOrWorld * ComponentToWorld) : LocalOrWorld;
		PositionSum = PositionSum + FVector(WorldMatrices[Index].M[3][0], WorldMatrices[Index].M[3][1], WorldMatrices[Index].M[3][2]);
	}

	// 2) 이미터 내부 정렬 (SortMode)
	TArray<int32> Order;
	BuildSortOrder(Count, Source.SortMode, View,
		[&](int32 Index) { return FVector(WorldMatrices[Index].M[3][0], WorldMatrices[Index].M[3][1], WorldMatrices[Index].M[3][2]); },
		[&](int32 Index) { return Index < Source.RelativeTimes.Num() ? Source.RelativeTimes[Index] : 0.0f; }, Order);

	// 3) 정렬 순서대로 인스턴스 데이터 채우기
	FParticleMeshInstance* Instances = Queue.AllocateInstances<FParticleMeshInstance>(static_cast<uint32>(Count));
	for (int32 DrawIndex = 0; DrawIndex < Count; ++DrawIndex)
	{
		Instances[DrawIndex].World = WorldMatrices[Order[DrawIndex]];
	}

	// 인스턴스마다 월드 행렬이 있으므로 오브젝트 상수(World)는 단위 행렬
	const FMatrix* IdentityModel = Queue.StoreWorldMatrix(FMatrix::Identity);
	const float EmitterDistSq = DistanceSqToCenter(PositionSum, Count, View);

	// 4) 이미터×섹션당 패킷 1개 = 드로우콜 1번 (섹션마다 머티리얼이 다를 수 있다)
	const auto AddPacket = [&](UMaterial* Material, uint32 StartIndex, uint32 IndexCount)
	{
		FRenderPacket& Packet = Queue.AddDefaulted_GetRef();
		Packet.Proxy = Proxy;
		Packet.Model = IdentityModel;
		Packet.Mesh = StaticMesh;
		Packet.VertexBuffer = VB;
		Packet.IndexBuffer = IB;
		Packet.Material = Material;
		Packet.CameraToParticleDistance = EmitterDistSq;
		Packet.StartIndex = StartIndex;
		Packet.IndexCount = IndexCount;
		Packet.InstanceData = Instances;
		Packet.InstanceStride = sizeof(FParticleMeshInstance);
		Packet.InstanceCount = static_cast<uint32>(Count);
	};

	if (MeshData.Sections.IsEmpty())
	{
		AddPacket(OverrideMaterial ? OverrideMaterial : StaticMesh->GetMaterial(0), 0, 0);
	}
	else
	{
		for (const FStaticMeshSection& Section : MeshData.Sections)
		{
			AddPacket(OverrideMaterial ? OverrideMaterial : StaticMesh->GetMaterial(Section.MaterialSlotIndex),
				Section.StartIndex, Section.IndexCount);
		}
	}
}
