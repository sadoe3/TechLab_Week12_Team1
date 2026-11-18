#pragma once

#include "RenderPacket.h"
#include "Render/ObjectConstants.h"
#include "Texture2D.h"
#include "Text/Font.h"
#include "SceneView.h"
#include "Occlusion/GPUOcclusion.h"
#include "Engine/LightSceneProxy.h"
#include "Render/Buffer.h"
#include "MeshPassProcessor.h"
#include <span>

class FSkyboxRenderer;
class FHeightFogRenderer;
class FFXAARenderer;
class FCameraOverlayRenderer;

class FTextRenderer;

class FRenderer
{
public:
	// 전방 선언한 패스 객체를 TUniquePtr로 소유하므로 정의는 cpp에 둔다.
	FRenderer();
	~FRenderer();

	bool Init();

	// 패스의 의미를 해석하지 않고 준비된 GPU 상태와 드로우만 제출한다.
	void UploadViewMatrix(const FMatrix& ViewProjection);
	void DrawMeshes(std::span<const FMeshDrawBatch> Batches);

	// 오클루전 객체 조회
	FGPUOcclusion& GetGPUOcclusion() { return GPUOcclusion; }

	// 측정 대상 드로우를 동일한 GPU 상태로 다시 제출한다.
	FOcclusionMeasureResult MeasureDrawVisibility(std::span<const FMeshDrawBatch> Batches);

	uint8* BeginObjectConstants(uint32 MaxSlots);
	void EndObjectConstants();

	void InitScenePasses(const FString& SkyPanoramaPath);

	// FSceneRenderer::Render가 쓰는 상주 패스 객체. UE에서는 렌더러 모듈의 글로벌 자원에 해당한다.
	FSkyboxRenderer* GetSkybox() const { return Skybox.get(); }
	FHeightFogRenderer* GetHeightFog() const { return HeightFog.get(); }
	FFXAARenderer* GetFXAA() const { return FXAA.get(); }
	FCameraOverlayRenderer* GetCameraOverlay() const { return CameraOverlay.get(); }
	FTextRenderer* GetSceneText() const { return SceneText.get(); }

	FConstantBuffer* GetToneMapConstants() const { return ToneMapConstants.get(); }
	FConstantBuffer* GetDecalConstants() const { return DecalConstants.get(); }

	// r.Particle.GPUInstancing. 켜면 파티클 배치를 DrawIndexedInstanced 한 번으로 그리고,
	// 끄면 파티클마다 인스턴스 데이터 1개를 올려 드로우콜을 따로 낸다 (인스턴싱 효과 비교용).
	inline static bool bParticleGPUInstancing = true;

	// 직전 프레임에 파티클(인스턴스) 배치가 낸 드로우콜 수. 모든 뷰포트와 패스(프리패스·그림자 포함) 합계.
	static uint32 GetParticleDrawCallsLastFrame() { return ParticleDrawCallsLastFrame; }
	// 프레임 끝에서 한 번 불러 이번 프레임 집계를 확정한다 (FStatRegistry::EndFrame 옆).
	static void EndFrameStats()
	{
		ParticleDrawCallsLastFrame = ParticleDrawCallsThisFrame;
		ParticleDrawCallsThisFrame = 0;
	}

private:
	void EnsurePerObjectSlotCapacity(uint32 SlotCount);
	void BindBatch(const FMeshDrawBatch& Batch);
	void BindObject(const FMeshDrawBatch& Batch, const FMeshDrawBatch::FItem& Item);
	// 배치의 인스턴스 데이터 중 [FirstInstance, FirstInstance + Count)를 stride별 동적 StructuredBuffer에 올리고 t15(VS)에 바인딩한다.
	bool BindInstances(const FMeshDrawBatch& Batch, uint32 FirstInstance, uint32 Count);

	inline static uint32 ParticleDrawCallsThisFrame = 0;
	inline static uint32 ParticleDrawCallsLastFrame = 0;

	// StructuredBuffer는 생성 시 stride가 고정되므로 stride마다 하나씩 둔다. 배치마다 Map-Discard로 덮어쓴다.
	struct FInstanceBufferSlot
	{
		uint32 Stride = 0;
		TUniquePtr<FStructuredBuffer> Buffer;
	};
	TArray<FInstanceBufferSlot> InstanceBuffers;

	TUniquePtr<FConstantBuffer> ObjectCB;
	TUniquePtr<FConstantBuffer> ViewCB;
	TUniquePtr<FConstantBuffer> MaterialCB;
	TUniquePtr<FConstantBuffer> PerObjectSlotCB;
	TUniquePtr<FConstantBuffer> ToneMapConstants;
	TUniquePtr<FConstantBuffer> DecalConstants;
	uint32 PerObjectSlotCapacity = 0;
	bool bUsePerObjectSlots = false;
	TArray<ComPtr<ID3D11Query>> OcclusionQueries;
	ComPtr<ID3D11DepthStencilState> DepthLessEqualReadOnly;
	FGPUOcclusion GPUOcclusion;

	TUniquePtr<FSkyboxRenderer> Skybox;
	TUniquePtr<FHeightFogRenderer> HeightFog;
	TUniquePtr<FFXAARenderer> FXAA;
	TUniquePtr<FCameraOverlayRenderer> CameraOverlay;
	TUniquePtr<FTextRenderer> SceneText;
};
