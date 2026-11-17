#include "EnginePCH.h"
#include "ParticleSystemSceneProxy.h"

#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleDynamicData.h"
#include "Render/RenderCommand.h"

FParticleSystemSceneProxy::FParticleSystemSceneProxy(UParticleSystemComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
}

void FParticleSystemSceneProxy::UpdateRenderState()
{
	// RenderState를 비워 두면 그림자 패스가 이 프록시를 건너뛴다
	RenderState.reset();
	UpdateCommonFlags();
}

void FParticleSystemSceneProxy::UpdateDynamicBuffers(const UParticleSystemComponent* PSC)
{
	// 버전 검사
	if (PSC->GetRenderDataVersion() == FilledVersion)
	{
		return;
	}

	const auto& RenderData = PSC->GetEmitterRenderData();
	EmitterVertexBuffers.SetNum(RenderData.Num());
	int32 MaxParticleCount = 0;

	for (const auto* Data : RenderData)
	{
		const FDynamicEmitterReplayDataBase& Source = Data->GetSource();
		const int32 MaxParticles = Source.DataContainer.ParticleIndicesNumShorts;
		MaxParticleCount = FMath::Max(MaxParticleCount, MaxParticles);

		// 지금은 Sprite만 정점을 채울 줄 안다
		if (Source.eEmitterType != EDynamicEmitterType::Sprite) continue;
		const FDynamicSpriteEmitterData* Sprite = static_cast<const FDynamicSpriteEmitterData*>(Data);

		// 최대 용량 기준으로 잡아야 파티클 수가 늘 때마다 다시 만들지 않는다
		const uint32 Stride = static_cast<uint32>(Sprite->GetDynamicVertexStride());
		const uint32 NeedBytes = static_cast<uint32>(MaxParticles) * 4 * Stride;
		TUniquePtr<FVertexBuffer>& VB = EmitterVertexBuffers[Data->EmitterIndex];
		if (!VB || VB->GetBufferSize() < NeedBytes)
		{
			VB = RenderCommand::CreateDynamicVertexBuffer(NeedBytes, Stride);
		}

		if (Source.ActiveParticleCount == 0 || !VB) continue;

		// WRITE_DISCARD라 이전 내용이 없으니 살아 있는 파티클을 전부 다시 쓴다
		FParticleSpriteVertex* Dst = static_cast<FParticleSpriteVertex*>(RenderCommand::MapWriteDiscard(VB.get()));
		if (!Dst) continue;
		Sprite->FillVertices(Dst);
		RenderCommand::Unmap(VB.get());
	}

	if (MaxParticleCount > 0 && (!IndexBuffer || IndexBuffer->GetIndexCount() < static_cast<uint32>(MaxParticleCount) * 6))
	{
		// 쿼드 패턴은 파티클 값과 무관하니 만들 때 한 번만 채우고 이후엔 앞부분만 잘라 쓴다
		const uint32 IndexCount = static_cast<uint32>(MaxParticleCount) * 6;
		TArray<uint32> Indices;
		Indices.SetNum(static_cast<int32>(IndexCount));
		for (uint32 i = 0; i < static_cast<uint32>(MaxParticleCount); ++i)
		{
			const uint32 Base = i * 4;
			uint32* Quad = &Indices[static_cast<int32>(i * 6)];
			Quad[0] = Base + 0; Quad[1] = Base + 1; Quad[2] = Base + 2;
			Quad[3] = Base + 0; Quad[4] = Base + 2; Quad[5] = Base + 3;
		}
		IndexBuffer = RenderCommand::CreateDynamicIndexBuffer(IndexCount);
		RenderCommand::UpdateBufferData(IndexBuffer.get(), Indices.GetData(), IndexCount * sizeof(uint32));
	}

	// 버전 갱신
	FilledVersion = PSC->GetRenderDataVersion();
}

void FParticleSystemSceneProxy::GetDynamicMeshElements(FRenderQueue& Queue, const FViewContext& View)
{
	const UParticleSystemComponent* PSC = static_cast<const UParticleSystemComponent*>(GetComponent());
	UpdateDynamicBuffers(PSC);

	if (!IndexBuffer) return;

	// 셰이더가 World[1]/World[2]를 카메라 Right/Up으로 읽는다. 단위 길이여야 Size가 그대로 크기가 된다
	const FMatrix* CameraAxes = nullptr;

	const FVector Delta = PSC->GetWorldLocation() - View.CameraPosition;
	const float DistanceSq = Delta.Dot(Delta);

	for (const FDynamicEmitterDataBase* Data : PSC->GetEmitterRenderData())
	{
		const FDynamicEmitterReplayDataBase& Source = Data->GetSource();
		if (Source.eEmitterType != EDynamicEmitterType::Sprite || Source.ActiveParticleCount == 0) continue;

		const FDynamicSpriteEmitterReplayDataBase& SpriteSource = static_cast<const FDynamicSpriteEmitterReplayDataBase&>(Source);
		FVertexBuffer* VB = EmitterVertexBuffers.IsValidIndex(Data->EmitterIndex) ? EmitterVertexBuffers[Data->EmitterIndex].get() : nullptr;
		if (!SpriteSource.Material || !VB) continue;

		if (!CameraAxes)
		{
			CameraAxes = Queue.StoreWorldMatrix(View.BuildBillboardMatrix(FVector::ZeroVector, 1.0f, 1.0f));
		}

		FRenderPacket& Packet = Queue.AddDefaulted_GetRef();
		// Proxy를 넣으면 렌더러가 b2에 LocalToWorld를 넣어 버리므로 비우고 Model로 카메라 축을 보낸다
		Packet.Proxy = nullptr;
		Packet.Model = CameraAxes;
		Packet.VertexBuffer = VB;
		Packet.IndexBuffer = IndexBuffer.get();
		Packet.Material = SpriteSource.Material;
		Packet.StartIndex = 0;
		Packet.IndexCount = static_cast<uint32>(Source.ActiveParticleCount) * 6;
		// 지금은 이미터 단위로만 정렬한다. 파티클 단위 정렬은 Phase 4
		Packet.CameraToParticleDistance = DistanceSq;
	}
}
