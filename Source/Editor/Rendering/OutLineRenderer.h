#pragma once

#include "Render/Renderer.h"
#include "Render/RenderingInfo.h"
#include "Render/PipelineState.h"
#include "Editor/Rendering/Outline.h"

// Outline의 월드·투영 행렬과 View별 픽셀 크기·확장 두께를 담는다.
struct FOutlineData
{
	FMatrix World;
	FMatrix NormalMatrix;
	FMatrix ViewProj;
	FVector4 ViewportAndThickness;
};

class FOutlineRenderer
{
public:

	// 스텐실 마스크·외곽선 두 패스의 Shader와 상수 버퍼를 준비한다.
	void Init(FRenderer* InRenderer);
	// 외부에서 지정한 Mesh 참조를 저장한다.
	void SetMesh(UStaticMesh* InMesh);
	// 원본 메시로 스텐실을 찍은 뒤, 확장 메시를 그 바깥에만 그려 외곽선만 남긴다.
	// View가 있으면 메시 에셋이 없는 컴포넌트(파티클)도 SceneProxy가 내놓는 렌더 패킷으로 외곽선을 그린다.
	void OnRender(const FOutline& InOutline, const FMatrix& ViewProj, const FViewportSettings& Viewport,
		const FViewContext* View = nullptr);

private:
	// 외곽선 대상 하나를 그릴 때 필요한 정보. 모든 대상을 모은 뒤 스텐실 패스, 외곽선 패스를 차례로 돈다.
	struct FOutlineDraw
	{
		FVertexBuffer* VertexBuffer = nullptr;
		FIndexBuffer* IndexBuffer = nullptr;
		uint32 IndexCount = 0;
		uint32 StartIndex = 0;
		uint32 InstanceCount = 0;           // 0이면 일반 드로우
		FMatrix World = FMatrix::Identity;
		FMatrix NormalMatrix = FMatrix::Identity;
		FShaderProgram* Shader = nullptr;   // 일반 / #GPU_SKIN / #PARTICLE_INSTANCE
		const FStructuredBuffer* Resource = nullptr; // 본 버퍼 또는 인스턴스 버퍼
		uint32 ResourceSlot = 0;
		float Mode = 0.0f;                  // OutlineShader.hlsl ViewportAndThickness.w
	};

	// 컴포넌트 하나를 그리기 목록에 넣는다. 스태틱 메시 → 스켈레탈 메시 → SceneProxy 렌더 패킷(파티클) 순으로 찾는다.
	void CollectDraws(UPrimitiveComponent* Target, const FViewContext* View, TArray<FOutlineDraw>& OutDraws);
	// SceneProxy::GetDynamicMeshElements로 받은 패킷(파티클 스프라이트/메시 인스턴스)을 그리기 목록에 넣는다.
	// (Unreal 선택 외곽선과 같은 방식: 선택된 프록시의 메시 배치를 그대로 다시 그린다)
	void CollectProxyDraws(UPrimitiveComponent* Target, const FViewContext& View, TArray<FOutlineDraw>& OutDraws);

	FRenderer* Renderer;

	FShaderProgram* Shader;
	FPipelineState MaskPipelineState;
	FPipelineState OutlinePipelineState;
	UStaticMesh* Mesh;

	ComPtr<ID3D11RasterizerState> RasterizerState;
	TUniquePtr<FConstantBuffer> ConstantBuffer;
	// 패킷마다 하나씩 쓰는 인스턴스 버퍼. 스텐실 패스를 모든 패킷에 먼저 끝내야 하므로 패킷별로 따로 올려 둔다.
	struct FInstanceBuffer
	{
		TUniquePtr<FStructuredBuffer> Buffer;
		uint32 Stride = 0;
	};
	TArray<FInstanceBuffer> InstanceBuffers;
	int32 UsedInstanceBuffers = 0; // 이번 프레임에 쓴 InstanceBuffers 개수
};
