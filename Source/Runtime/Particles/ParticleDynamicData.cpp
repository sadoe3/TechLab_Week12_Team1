#include "EnginePCH.h"
#include "Particles/ParticleDynamicData.h"
#include "Engine/ParticleSystemSceneProxy.h"
#include "Render/Mesh.h"
#include "Render/RenderPacket.h"
#include <algorithm>

void FDynamicSpriteEmitterData::BuildParticleParams()
{
	ParticleParams.Empty();
	ParticleParams.Reserve(Source.Particles.Num());
	for (const FDynamicSpriteEmitterReplayData::FSpriteParticleSnapshot& Particle : Source.Particles)
	{
		// CurrentFrame, AtlasRowSize(세로 칸 수), AtlasColSize(가로 칸 수), Alpha
		// SubUV 애니메이션이 아직 없으므로 아틀라스의 0번 칸만 그린다. (ParticleSubUVShader가 칸을 고르고, BillboardShader는 무시)
		FSubUVConstants Params{ 0.0f,
			static_cast<float>(std::max(SubImagesVertical, 1)),
			static_cast<float>(std::max(SubImagesHorizontal, 1)),
			Particle.Color.W };
		Params.Tint = FVector4(Particle.Color.X, Particle.Color.Y, Particle.Color.Z, 1.0f);
		// 반투명 패스는 톤 매핑 이후 LDR SceneColor에 그려지므로 Billboard와 동일하게 sRGB 디코드를 끈다.
		Params.DecodeSRGB = 0.0f;
		ParticleParams.Add(Params);
	}
}

void FDynamicSpriteEmitterData::GetDynamicMeshElements(
	const FParticleSystemSceneProxy* Proxy,
	const FViewContext& View,
	FRenderQueue& Queue) const
{
	if (!Proxy || !QuadMesh || !Material || Source.Particles.IsEmpty()
		|| ParticleParams.Num() != Source.Particles.Num())
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

	for (int32 ParticleIndex = 0; ParticleIndex < Source.Particles.Num(); ++ParticleIndex)
	{
		const FDynamicSpriteEmitterReplayData::FSpriteParticleSnapshot& Particle = Source.Particles[ParticleIndex];
		const FSubUVConstants* ParamData = &ParticleParams[ParticleIndex];
		FVector WorldPos = Particle.Location;
		if (Source.bUseLocalSpace)
		{
			const FVector4 Transformed = FVector4(WorldPos.X, WorldPos.Y, WorldPos.Z, 1.0f) * ComponentToWorld;
			WorldPos = FVector(Transformed.X, Transformed.Y, Transformed.Z);
		}

		const FMatrix BillboardMat = View.BuildBillboardMatrix(WorldPos, Particle.Size.X, Particle.Size.Y);
		const FMatrix* ModelPtr = Queue.StoreWorldMatrix(BillboardMat);
		const FVector Delta = WorldPos - View.CameraPosition;
		const float DistSq = Delta.Dot(Delta);

		if (MeshData.Sections.IsEmpty())
		{
			FRenderPacket& Packet = Queue.AddDefaulted_GetRef();
			Packet.Proxy = Proxy;
			Packet.Model = ModelPtr;
			Packet.Mesh = QuadMesh;
			Packet.VertexBuffer = VB;
			Packet.IndexBuffer = IB;
			Packet.Material = Material;
			Packet.MaterialParamData = ParamData;
			Packet.MaterialParamDataSize = sizeof(FSubUVConstants);
			Packet.CameraToParticleDistance = DistSq;
			Packet.StartIndex = 0;
			Packet.IndexCount = 0;
		}
		else
		{
			for (const FStaticMeshSection& Section : MeshData.Sections)
			{
				FRenderPacket& Packet = Queue.AddDefaulted_GetRef();
				Packet.Proxy = Proxy;
				Packet.Model = ModelPtr;
				Packet.Mesh = QuadMesh;
				Packet.VertexBuffer = VB;
				Packet.IndexBuffer = IB;
				Packet.Material = Material;
				Packet.MaterialParamData = ParamData;
				Packet.MaterialParamDataSize = sizeof(FSubUVConstants);
				Packet.CameraToParticleDistance = DistSq;
				Packet.StartIndex = Section.StartIndex;
				Packet.IndexCount = Section.IndexCount;
			}
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

	for (const FMatrix& LocalOrWorldMat : Source.ParticleMatrices)
	{
		const FMatrix FinalMat = Source.bUseLocalSpace
			? (LocalOrWorldMat * ComponentToWorld)
			: LocalOrWorldMat;
		const FMatrix* ModelPtr = Queue.StoreWorldMatrix(FinalMat);

		if (MeshData.Sections.IsEmpty())
		{
			FRenderPacket& Packet = Queue.AddDefaulted_GetRef();
			Packet.Proxy = Proxy;
			Packet.Model = ModelPtr;
			Packet.Mesh = StaticMesh;
			Packet.VertexBuffer = VB;
			Packet.IndexBuffer = IB;
			Packet.Material = OverrideMaterial ? OverrideMaterial : StaticMesh->GetMaterial(0);
			Packet.StartIndex = 0;
			Packet.IndexCount = 0;
		}
		else
		{
			for (const FStaticMeshSection& Section : MeshData.Sections)
			{
				FRenderPacket& Packet = Queue.AddDefaulted_GetRef();
				Packet.Proxy = Proxy;
				Packet.Model = ModelPtr;
				Packet.Mesh = StaticMesh;
				Packet.VertexBuffer = VB;
				Packet.IndexBuffer = IB;
				Packet.Material = OverrideMaterial ? OverrideMaterial : StaticMesh->GetMaterial(Section.MaterialSlotIndex);
				Packet.StartIndex = Section.StartIndex;
				Packet.IndexCount = Section.IndexCount;
			}
		}
	}
}
