#pragma once

#include "Shader.h"
#include "PipelineState.h"

// 렌더링에 사용할 파이프라인 종류 정의
enum class EPSOType : uint8
{
	StaticMesh_Opaque,       // 기본 불투명
	StaticMesh_Translucent,  // 반투명
	StaticMesh_Wireframe,    
	Particle_AlphaBlend,     
	Particle_Additive,       
	Skybox,                  
	Grid,                    
	Outline_Mask,            
	Outline_Draw,
	StaticMesh_GBuffer,
	DeferredLighting,
	DeferredPointLighting,      // 이전 재질 파일의 PSOType 숫자와 호환을 위해 ID 유지
	ToneMap,
	Reserved_13,                // 삭제된 FireBall 자리. 씬 파일의 PSOType 숫자(Billboard=14 등)가 밀리지 않게 남긴다
	Billboard,
	StaticMesh_Forward,
	StaticMesh_WorldNormal,
	StaticMesh_DepthOnly,
	StaticMesh_LightShadow,
	LightCullingDebug,
	ShadowTileClear,         // 그림자 캐시: 다시 그릴 아틀라스 타일만 깊이 1로 지운다
	ShadowDepth,
	ShadowProjection,        // 그림자 한 장을 화면 마스크(ScreenShadowMask)에 그린다
	ShadowMaskVisualize,     // Shadow Mask 뷰 모드: 마스크를 화면에 띄운다
	Decal, 
	DecalAdditive,
	Particle_Ribbon,         // 리본 띠 (FParticleRibbonVertex 동적 VB)
	Count
};

class FRenderResourceManager
{
public:
	static FRenderResourceManager& Get()
	{
		static FRenderResourceManager* Instance = new FRenderResourceManager();
		return *Instance;
	}

	static void Init()
	{
		Get().ScanShaders("Resources/Shader");
		Get().InitPipelineStates(); // PSO 초기화 추가
	}

	void ScanShaders(const fs::path& ShaderRoot);
	static void Shutdown();
	static FShaderProgram* GetShaderProgram(const FString& InPath);

	static FPipelineState* GetPSO(const EPSOType& InType);
	static FShaderProgram* GetForwardShaderProgram(ELightingModel Model);
	// GetShaderProgram과 달리 없으면 DefaultShader로 대체하지 않고 nullptr를 돌려준다 (변형 존재 여부 확인용).
	static FShaderProgram* FindShaderProgram(const FString& InName);

	static constexpr const char* ParticleInstanceSuffix = "#PARTICLE_INSTANCE";
private:

	void LoadOrCompileShader(const FString& Path);
	void CreateSkinnedVariant(const FString& Path, const char* LightingMacro, const FShaderProgram& Base);
	// 매크로 하나(VariantMacro=1)를 더 켜서 VS만 다시 컴파일하고 Base.Name + Suffix로 등록한다. PS는 Base 것을 함께 쓴다.
	void CreateVertexShaderVariant(const FString& Path, const char* LightingMacro, const FShaderProgram& Base,
		const char* VariantMacro, const char* Suffix);
	// 소스에 마커가 있으면 그 셰이더의 변형(GPU 스킨, 파티클 인스턴스)을 만든다.
	void CreateVariantsFromSource(const FString& Path, const std::string& Source, const char* LightingMacro, const FShaderProgram& Base);
	void InitPipelineStates();

	TMap<FString, TUniquePtr<FVertexShader>> VertexShaderMap;
	TMap<FString, TUniquePtr<FPixelShader>>  PixelShaderMap;
	TMap<FString, TUniquePtr<FShaderProgram>> ShaderProgramMap;

	TMap<EPSOType, TUniquePtr<FPipelineState>> PipelineStateMap;
};
