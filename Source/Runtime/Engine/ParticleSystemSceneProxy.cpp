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

	for (const FDynamicEmitterDataBase* Data : PSC->GetEmitterRenderData())
	{
		// TODO: Step 5 - 이미터마다 정점을 채우고 패킷 1개를 제출한다
	}
}
