#include "EnginePCH.h"
#include "Editor/Rendering/OutLineRenderer.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"
#include "Render/RenderResourceManager.h"
#include "Components/SkinnedMeshComponent.h"
#include "Render/SkeletalMeshObject.h"
#include "Render/SceneView.h"
#include "Render/RenderPacket.h"
#include "Engine/PrimitiveSceneProxy.h"
#include "Particles/ParticleDynamicData.h"

namespace
{
	// OutlineShader.hlsl의 OutlineSpriteInstances(t16). 메시 인스턴스는 공용 슬롯 t15(ParticleInstanceBufferSlot)를 쓴다.
	constexpr uint32 OutlineSpriteInstanceSlot = 16;

	// OutlineShader.hlsl ViewportAndThickness.w 값
	constexpr float OutlineModeMesh = 0.0f;
	constexpr float OutlineModeMeshInstance = 1.0f;
	constexpr float OutlineModeSpriteInstance = 2.0f;
}

// 스텐실 마스크·외곽선 두 패스의 Shader와 상수 버퍼를 준비한다.
// 두 패스 모두 깊이 테스트를 끄므로 다른 물체에 가려져도 외곽선이 보인다.
void FOutlineRenderer::Init(FRenderer* InRenderer)
{
	Renderer = InRenderer;
	Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/OutlineShader.hlsl");

	// 1패스: 원본 크기 메시 영역을 색 없이 스텐실에만 기록한다.
	MaskPipelineState.Shader = Shader;
	MaskPipelineState.Topology = EPrimitiveTopology::TriangleList;
	MaskPipelineState.RasterizerState = ERasterizerState::SolidNone;
	MaskPipelineState.BlendState = EBlendState::NoColorWrite;
	MaskPipelineState.DepthStencilState = EDepthStencilState::StencilMask;

	// 2패스: 확장 메시를 스텐실이 비어 있는 곳에만 그려 테두리 띠만 남긴다.
	OutlinePipelineState.Shader = Shader;
	OutlinePipelineState.Topology = EPrimitiveTopology::TriangleList;
	OutlinePipelineState.RasterizerState = ERasterizerState::SolidNone;
	OutlinePipelineState.BlendState = EBlendState::Opaque;
	OutlinePipelineState.DepthStencilState = EDepthStencilState::StencilOutline;

	static_assert(sizeof(FOutlineData) == 208, "Outline constant buffer layout mismatch");
	ConstantBuffer = RenderCommand::CreateConstantBuffer(sizeof(FOutlineData));
}

// 외부에서 지정한 Mesh 참조를 저장한다.
void FOutlineRenderer::SetMesh(UStaticMesh* InMesh)
{
	Mesh = InMesh;
}

// 카메라 거리 대신 View별 픽셀 크기와 고정 확장량으로 선택 Outline을 그린다.
// 대상이 여러 개면(액터의 Primitive 전부) 모두 스텐실을 먼저 찍고 외곽선을 그려, 합친 실루엣의 바깥만 남긴다.
void FOutlineRenderer::OnRender(const FOutline& InOutline, const FMatrix& InViewProj, const FViewportSettings& Viewport,
	const FViewContext* View)
{
	if (InOutline.GetTargets().IsEmpty() || Viewport.Width == 0 || Viewport.Height == 0)
	{
		return;
	}

	UsedInstanceBuffers = 0;
	TArray<FOutlineDraw> Draws;
	for (UPrimitiveComponent* Target : InOutline.GetTargets())
	{
		if (Target)
		{
			CollectDraws(Target, View, Draws);
		}
	}
	// 그릴 것이 없는 컴포넌트(텍스트 등)만 선택됐으면 아웃라인을 그리지 않는다
	if (Draws.IsEmpty())
	{
		return;
	}

	RenderCommand::BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Vertex);
	const float ViewportWidth = static_cast<float>(Viewport.Width);
	const float ViewportHeight = static_cast<float>(Viewport.Height);

	const auto DrawAll = [&](const FPipelineState& BasePipeline, float Thickness)
	{
		for (const FOutlineDraw& Draw : Draws)
		{
			FPipelineState Pipeline = BasePipeline;
			Pipeline.Shader = Draw.Shader;
			RenderCommand::BindPipelineState(Pipeline);
			if (Draw.Resource)
			{
				RenderCommand::BindShaderResource(Draw.ResourceSlot, Draw.Resource, EShaderBindFlagBits::Vertex);
			}
			RenderCommand::BindVertexBuffer(Draw.VertexBuffer);
			RenderCommand::BindIndexBuffer(Draw.IndexBuffer);

			const FOutlineData Data = { Draw.World, Draw.NormalMatrix, InViewProj,
				FVector4(ViewportWidth, ViewportHeight, Thickness, Draw.Mode) };
			RenderCommand::UpdateBufferData(ConstantBuffer.get(), &Data, sizeof(Data));

			if (Draw.InstanceCount > 0)
			{
				RenderCommand::DrawIndexedInstanced(Draw.IndexCount, Draw.InstanceCount, Draw.StartIndex);
			}
			else
			{
				RenderCommand::DrawIndexed(Draw.IndexCount, Draw.StartIndex);
			}
		}
	};

	// 1패스: 두께 0(확장 없음)으로 선택 대상 영역을 전부 스텐실에 찍는다.
	DrawAll(MaskPipelineState, 0.0f);
	// 2패스: 확장한 대상 중 스텐실 바깥 부분만 노란색으로 그린다.
	DrawAll(OutlinePipelineState, 2.0f);

	// 이후 패스(Gizmo 등)에 스텐실·블렌드 상태가 새지 않도록 기본값으로 되돌린다.
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
}

void FOutlineRenderer::CollectDraws(UPrimitiveComponent* Target, const FViewContext* View, TArray<FOutlineDraw>& OutDraws)
{
	if (!Target->IsVisible())
	{
		return;
	}

	FOutlineDraw Draw;
	Draw.Shader = Shader;
	Draw.World = Target->GetWorldMatrix();
	Draw.NormalMatrix = Draw.World.Inverse().GetTransposed();
	Draw.Mode = OutlineModeMesh;

	if (UStaticMesh* StaticMesh = Target->GetRenderMesh())
	{
		Draw.VertexBuffer = StaticMesh->GetVertexBuffer(0);
		Draw.IndexBuffer = StaticMesh->GetIndexBuffer(0);
	}
	else if (USkinnedMeshComponent* Skinned = Cast<USkinnedMeshComponent>(Target))
	{
		// CPU 경로는 현재 포즈가 들어 있는 인스턴스 VB, GPU 경로는 공유 VB와 본 버퍼로 그린다
		if (FSkeletalMeshObject* MeshObject = Skinned->GetMeshObject())
		{
			Draw.VertexBuffer = MeshObject->GetVertexBuffer(0);
			Draw.IndexBuffer = MeshObject->GetIndexBuffer(0);
			if (const FStructuredBuffer* BoneBuffer = MeshObject->GetBoneMatrixBuffer())
			{
				Draw.Shader = FRenderResourceManager::GetShaderProgram(Shader->Name + "#GPU_SKIN");
				Draw.Resource = BoneBuffer;
				Draw.ResourceSlot = BoneMatrixBufferSlot;
			}
		}
	}
	else
	{
		// 메시 에셋이 없는 컴포넌트: 파티클처럼 SceneProxy가 렌더 패킷을 만드는 경우만 그 패킷으로 그린다.
		if (View)
		{
			CollectProxyDraws(Target, *View, OutDraws);
		}
		return;
	}

	if (!Draw.VertexBuffer || !Draw.IndexBuffer || !Draw.Shader)
	{
		return;
	}
	Draw.IndexCount = Draw.IndexBuffer->GetIndexCount();
	OutDraws.Add(Draw);
}

void FOutlineRenderer::CollectProxyDraws(UPrimitiveComponent* Target, const FViewContext& View, TArray<FOutlineDraw>& OutDraws)
{
	FPrimitiveSceneProxy* Proxy = Target->GetSceneProxy();
	if (!Proxy || !Proxy->HasDynamicMeshElements())
	{
		return;
	}

	// 이번 뷰 기준 패킷을 다시 모은다. 스프라이트는 이 뷰의 카메라 축으로 세워진다.
	FViewContext Context = View;
	Context.bShadowDepthPass = false;
	FRenderQueue Queue;
	Proxy->GetDynamicMeshElements(Queue, Context);
	if (Queue.IsEmpty())
	{
		return;
	}

	FShaderProgram* InstanceShader = FRenderResourceManager::FindShaderProgram(Shader->Name + FRenderResourceManager::ParticleInstanceSuffix);

	for (const FRenderPacket& Packet : Queue)
	{
		if (!Packet.VertexBuffer || !Packet.IndexBuffer)
		{
			continue;
		}

		FOutlineDraw Draw;
		Draw.VertexBuffer = Packet.VertexBuffer;
		Draw.IndexBuffer = Packet.IndexBuffer;
		Draw.IndexCount = Packet.IndexCount > 0 ? Packet.IndexCount : Packet.IndexBuffer->GetIndexCount();
		Draw.StartIndex = Packet.StartIndex;
		// 메시 인스턴스는 단위 행렬, 스프라이트는 카메라 축 행렬이 Model로 들어 있다.
		// 큐는 이 함수가 끝나면 사라지므로 행렬은 값으로 복사해 둔다.
		Draw.World = Packet.Model ? *Packet.Model : FMatrix::Identity;
		Draw.Shader = Shader;

		if (Packet.InstanceData && Packet.InstanceCount > 0)
		{
			if (!InstanceShader)
			{
				continue;
			}
			if (Packet.InstanceStride == sizeof(FParticleMeshInstance))
			{
				Draw.Mode = OutlineModeMeshInstance;
				Draw.ResourceSlot = ParticleInstanceBufferSlot;
			}
			else if (Packet.InstanceStride == sizeof(FParticleSpriteInstance))
			{
				Draw.Mode = OutlineModeSpriteInstance;
				Draw.ResourceSlot = OutlineSpriteInstanceSlot;
			}
			else
			{
				continue; // 모르는 인스턴스 형식
			}

			// 인스턴스 데이터는 두 패스에서 같이 쓰므로 패킷마다 따로 GPU에 올려 둔다.
			while (UsedInstanceBuffers >= InstanceBuffers.Num())
			{
				InstanceBuffers.Add(FInstanceBuffer{});
			}
			FInstanceBuffer& Slot = InstanceBuffers[UsedInstanceBuffers];
			// 크기·형식이 맞지 않으면 다시 만든다 (최소 256개, 2배씩)
			if (!Slot.Buffer || Slot.Buffer->GetCapacity() < Packet.InstanceCount || Slot.Stride != Packet.InstanceStride)
			{
				uint32 Capacity = 256u;
				while (Capacity < Packet.InstanceCount) Capacity *= 2;
				Slot.Buffer = RenderCommand::CreateDynamicStructuredBuffer(Capacity, Packet.InstanceStride);
				Slot.Stride = Packet.InstanceStride;
				if (!Slot.Buffer)
				{
					continue;
				}
			}
			++UsedInstanceBuffers;
			RenderCommand::UpdateBufferData(Slot.Buffer.get(), Packet.InstanceData, Packet.InstanceCount * Packet.InstanceStride);

			Draw.Shader = InstanceShader;
			Draw.Resource = Slot.Buffer.get();
			Draw.InstanceCount = Packet.InstanceCount;
			Draw.NormalMatrix = FMatrix::Identity;
		}
		else
		{
			Draw.NormalMatrix = Draw.World.Inverse().GetTransposed();
		}
		OutDraws.Add(Draw);
	}
}

// 그리는 도구를 만들고 셰이더와 상수 버퍼를 준비한다. FOutlineRenderer::Init의 렌더러 인자는 쓰이지 않는다.
FOutline::FOutline()
{
	Renderer = MakeUnique<FOutlineRenderer>();
	Renderer->Init(nullptr);
}

// FOutlineRenderer가 완전한 타입인 이 파일에서 소멸자를 정의해야 TUniquePtr가 지울 수 있다.
FOutline::~FOutline() = default;

void FOutline::Render(const FSceneView* View, const FViewportSettings& Viewport) const
{
	const FViewContext Context = View->ToViewContext();
	Renderer->OnRender(*this, View->ViewProjectionMatrix, Viewport, &Context);
}
