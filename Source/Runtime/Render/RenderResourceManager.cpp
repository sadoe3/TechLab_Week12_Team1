#include "EnginePCH.h"
#include "RenderResourceManager.h"

#include "RenderCommand.h"

#include "RenderUtil.h"
#include "Core/Misc/Paths.h"
#include <cstring>
#include <fstream>
#include <iterator>



struct FPipelineTableEntry
{
	EPSOType Type;
	const char* ShaderPath;
	EPrimitiveTopology Topology;
	ERasterizerState RasterizerState;
	EBlendState BlendState;
	EDepthStencilState DepthStencilState;
};

constexpr FPipelineTableEntry PipelineTable[] =
{
	{ EPSOType::StaticMesh_Opaque,       nullptr,                                            EPrimitiveTopology::TriangleList, ERasterizerState::SolidBack, EBlendState::Opaque,       EDepthStencilState::Default },
	{ EPSOType::StaticMesh_Translucent,  "Resources/Shader/StaticMeshTranslucentShader.hlsl", EPrimitiveTopology::TriangleList, ERasterizerState::SolidBack, EBlendState::AlphaBlend,   EDepthStencilState::ReadOnly },
	{ EPSOType::StaticMesh_Wireframe,    nullptr,                                    EPrimitiveTopology::TriangleList, ERasterizerState::Wireframe, EBlendState::Opaque,       EDepthStencilState::Default },
	{ EPSOType::Particle_AlphaBlend,     "Resources/Shader/ParticleSpriteShader.hlsl", EPrimitiveTopology::TriangleList, ERasterizerState::SolidNone, EBlendState::AlphaBlend,   EDepthStencilState::ReadOnly },
	{ EPSOType::Particle_Additive,       "Resources/Shader/ParticleSpriteShader.hlsl", EPrimitiveTopology::TriangleList, ERasterizerState::SolidNone, EBlendState::Additive,     EDepthStencilState::ReadOnly },
	{ EPSOType::Skybox,                  "Resources/Shader/SkyboxShader.hlsl",        EPrimitiveTopology::TriangleList, ERasterizerState::SolidNone, EBlendState::Opaque,       EDepthStencilState::ReadOnly },
	{ EPSOType::Grid,                    "Resources/Shader/GridShader.hlsl",          EPrimitiveTopology::TriangleList, ERasterizerState::SolidBack, EBlendState::AlphaBlend,   EDepthStencilState::Default },
	{ EPSOType::Outline_Mask,            "Resources/Shader/OutlineShader.hlsl",       EPrimitiveTopology::TriangleList, ERasterizerState::SolidNone, EBlendState::NoColorWrite, EDepthStencilState::StencilMask },
	{ EPSOType::Outline_Draw,            "Resources/Shader/OutlineShader.hlsl",       EPrimitiveTopology::TriangleList, ERasterizerState::SolidNone, EBlendState::Opaque,       EDepthStencilState::StencilOutline },
	{ EPSOType::StaticMesh_GBuffer,		 "Resources/Shader/StaticMeshGBufferShader.hlsl",	EPrimitiveTopology::TriangleList,	ERasterizerState::SolidBack,	EBlendState::Opaque,  EDepthStencilState::Default },
	{ EPSOType::DeferredLighting,       "Resources/Shader/DeferredLightingShader.hlsl",    EPrimitiveTopology::TriangleList, ERasterizerState::SolidNone, EBlendState::Opaque,       EDepthStencilState::Disabled },
	{ EPSOType::DeferredPointLighting,  "Resources/Shader/DeferredLightingShader.hlsl",    EPrimitiveTopology::TriangleList, ERasterizerState::SolidNone, EBlendState::Additive,     EDepthStencilState::Disabled },
	{ EPSOType::ToneMap,                "Resources/Shader/ToneMapShader.hlsl",              EPrimitiveTopology::TriangleList, ERasterizerState::SolidNone, EBlendState::Opaque,       EDepthStencilState::Disabled },
	{ EPSOType::Reserved_13,            nullptr,                                            EPrimitiveTopology::TriangleList, ERasterizerState::SolidNone, EBlendState::Opaque,       EDepthStencilState::Default },
	{ EPSOType::Billboard,              "Resources/Shader/BillboardShader.hlsl",            EPrimitiveTopology::TriangleList, ERasterizerState::SolidNone, EBlendState::AlphaBlend,   EDepthStencilState::ReadOnly },
	{ EPSOType::StaticMesh_Forward,		"Resources/Shader/UberLit.hlsl",			EPrimitiveTopology::TriangleList,			ERasterizerState::SolidBack, EBlendState::Opaque, EDepthStencilState::Default },
	{ EPSOType::StaticMesh_WorldNormal, "Resources/Shader/UberLit.hlsl", EPrimitiveTopology::TriangleList, ERasterizerState::SolidBack, EBlendState::Opaque, EDepthStencilState::Default },
	{ EPSOType::StaticMesh_DepthOnly, "Resources/Shader/DepthPrepass.hlsl", EPrimitiveTopology::TriangleList, ERasterizerState::SolidBack, EBlendState::NoColorWrite, EDepthStencilState::Default },
	{ EPSOType::StaticMesh_LightShadow, "Resources/Shader/DepthPrepass.hlsl", EPrimitiveTopology::TriangleList, ERasterizerState::LightShadow, EBlendState::NoColorWrite, EDepthStencilState::Default },
	{ EPSOType::LightCullingDebug, "Resources/Shader/LightCullingDebug.hlsl", EPrimitiveTopology::TriangleList, ERasterizerState::SolidNone, EBlendState::AlphaBlend, EDepthStencilState::Disabled },
	{ EPSOType::ShadowTileClear, nullptr, EPrimitiveTopology::TriangleList, ERasterizerState::SolidNone, EBlendState::NoColorWrite, EDepthStencilState::AlwaysWrite },
	{ EPSOType::ShadowDepth, "Resources/Shader/ShadowDepth.hlsl", EPrimitiveTopology::TriangleList, ERasterizerState::LightShadow, EBlendState::NoColorWrite, EDepthStencilState::Default },
	{ EPSOType::ShadowProjection, "Resources/Shader/ShadowProjection.hlsl", EPrimitiveTopology::TriangleList, ERasterizerState::SolidNone, EBlendState::ShadowMaskR, EDepthStencilState::Disabled },
	{ EPSOType::ShadowMaskVisualize, "Resources/Shader/ShadowMaskVisualize.hlsl", EPrimitiveTopology::TriangleList, ERasterizerState::SolidNone, EBlendState::Opaque, EDepthStencilState::Disabled },
	{ EPSOType::Decal,			"Resources/Shader/DecalShader.hlsl",			EPrimitiveTopology::TriangleList,		ERasterizerState::SolidBack, EBlendState::AlphaBlend, EDepthStencilState::ReadOnly },
	{ EPSOType::DecalAdditive,	"Resources/Shader/DecalShader.hlsl",			EPrimitiveTopology::TriangleList,		ERasterizerState::SolidBack, EBlendState::Additive, EDepthStencilState::ReadOnly },
};

static_assert(sizeof(PipelineTable) / sizeof(PipelineTable[0]) == static_cast<size_t>(EPSOType::Count));

void FRenderResourceManager::InitPipelineStates()
{
	for (size_t i = 0; i < static_cast<size_t>(EPSOType::Count); ++i)
	{
		const auto& Entry = PipelineTable[i];
		const EPSOType Type = static_cast<EPSOType>(i);
		PipelineStateMap[Type] = MakeUnique<FPipelineState>();
		FPipelineState* State = PipelineStateMap[Type].get();
		// World Normal은 전용 매크로 변형이 성공했을 때만 연결한다.
		// 셰이더가 없는 항목은 번호만 남긴 예약 자리다.
		State->Shader = Type == EPSOType::StaticMesh_WorldNormal || !Entry.ShaderPath ? nullptr : GetShaderProgram(Entry.ShaderPath);
		State->Topology = Entry.Topology;
		State->RasterizerState = Entry.RasterizerState;
		State->BlendState = Entry.BlendState;
		State->DepthStencilState = Entry.DepthStencilState;
	}
	const char* Names[] = { "LIGHTING_MODEL_GOURAUD", "LIGHTING_MODEL_LAMBERT", "LIGHTING_MODEL_PHONG", "LIGHTING_MODEL_UNLIT", "VIEW_MODE_WORLD_NORMAL" };
	for (const char* Name : Names)
	{
		const D3D_SHADER_MACRO Defines[] = { { Name, "1" }, { nullptr, nullptr } };
		const FString Path = "Resources/Shader/UberLit.hlsl";
		// 변형도 .cso 캐시를 쓴다(매크로 이름이 캐시 파일 이름에 들어간다). 매번 컴파일하면 Release에서 시작이 크게 늦어진다.
		FString VSCSOPath;
		FString PSCSOPath;
		FShaderByteCode VSCode = RenderUtil::GetOrCompile(Path, "mainVS", EShaderType::Vertex, VSCSOPath, Defines);
		FShaderByteCode PSCode = RenderUtil::GetOrCompile(Path, "mainPS", EShaderType::Pixel, PSCSOPath, Defines);
		if (!VSCode.IsValid() || !PSCode.IsValid())
		{
			HTR_LOG(Error, "[Shader] UberShader variant compile failed: {}", Name);
			continue;
		}
		auto VS = RenderCommand::CreateVertexShader(VSCode);
		auto PS = RenderCommand::CreatePixelShader(PSCode);
		if (!VS || !VS->IsValid() || !PS || !PS->IsValid())
		{
			continue;
		}
		FVertexShader* VSRaw = VS.get();
		FPixelShader* PSRaw = PS.get();
		const FString Key = Path + "#" + Name;
		VertexShaderMap[Key + "#VS"] = std::move(VS);
		PixelShaderMap[Key + "#PS"] = std::move(PS);
		ShaderProgramMap[Key] = MakeUnique<FShaderProgram>(VSRaw, PSRaw);
		ShaderProgramMap[Key]->Name = Key;
		CreateSkinnedVariant(Path, Name, *ShaderProgramMap[Key]);
		CreateVertexShaderVariant(Path, Name, *ShaderProgramMap[Key], "PARTICLE_INSTANCE", ParticleInstanceSuffix);
		if (std::strcmp(Name, "VIEW_MODE_WORLD_NORMAL") == 0)
			PipelineStateMap[EPSOType::StaticMesh_WorldNormal]->Shader = ShaderProgramMap[Key].get();
	}
}

FShaderProgram* FRenderResourceManager::GetForwardShaderProgram(ELightingModel Model)
{
	const char* Name = "LIGHTING_MODEL_LAMBERT";
	switch (Model)
	{
	case ELightingModel::Gouraud: Name = "LIGHTING_MODEL_GOURAUD"; break;
	case ELightingModel::Phong: Name = "LIGHTING_MODEL_PHONG"; break;
	case ELightingModel::Unlit: Name = "LIGHTING_MODEL_UNLIT"; break;
	default: break;
	}
	const FString Key = FString("Resources/Shader/UberLit.hlsl#") + Name;
	if (auto* Found = Get().ShaderProgramMap.Find(Key)) return Found->get();
	return nullptr;
}

FPipelineState* FRenderResourceManager::GetPSO(const EPSOType& InType)
{
	if (auto* Found = Get().PipelineStateMap.Find(InType))
	{
		return Found->get();
	}
	return nullptr;
}

void FRenderResourceManager::ScanShaders(const fs::path& ShaderRoot)
{
	std::error_code ErrorCode;

	// 셰이더는 엔진 폴더에 있다. 작업 디렉터리는 프로젝트 폴더이므로 엔진 폴더 기준으로 찾는다.
	const fs::path AbsoluteRoot = FPaths::EngineFile(ShaderRoot);
	if (!fs::exists(AbsoluteRoot, ErrorCode))
	{
		HTR_LOG(Error, "[Shader] Scan Root Not Found : {}", FPaths::ToUtf8(AbsoluteRoot));
		return;
	}

	for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(AbsoluteRoot))
	{
		if (!Entry.is_regular_file()) continue;
		if (Entry.path().extension() != ".hlsl") continue;
		// 컴퓨트 전용 .hlsl에는 mainVS/mainPS가 없다. VS/PS로 컴파일하지 않는다.
		std::ifstream SourceFile(Entry.path(), std::ios::binary);
		if (!SourceFile) continue;
		const std::string Source(std::istreambuf_iterator<char>{ SourceFile }, std::istreambuf_iterator<char>{});
		if (Source.find("mainVS(") == std::string::npos ||
			Source.find("mainPS(") == std::string::npos) continue;

		// 키는 지금까지처럼 엔진 상대 경로("Resources/Shader/X.hlsl")로 둔다.
		FString Path = FPaths::ToUtf8(Entry.path().lexically_relative(FPaths::EngineDir()));
		LoadOrCompileShader(Path);
		if (TUniquePtr<FShaderProgram>* Base = ShaderProgramMap.Find(Path))
			CreateVariantsFromSource(Path, Source, nullptr, **Base);
	}
}

void FRenderResourceManager::Shutdown()
{
	Get().VertexShaderMap.Empty();
	Get().PixelShaderMap.Empty();
	Get().ShaderProgramMap.Empty();
}

FShaderProgram* FRenderResourceManager::GetShaderProgram(const FString& InPath)
{
	if (TUniquePtr<FShaderProgram>* Found = Get().ShaderProgramMap.Find(InPath))
		return Found->get();

	HTR_LOG(Error, "[Shader] not found: {}", InPath);

	if (TUniquePtr<FShaderProgram>* Fallback =
		Get().ShaderProgramMap.Find("Resources/Shader/DefaultShader.hlsl"))
		return Fallback->get();

	return nullptr;
}



void FRenderResourceManager::LoadOrCompileShader(const FString& Path)
{ 
	FString VSCSOPath;
	FString PSCSOPath;
	FShaderByteCode VSCode = RenderUtil::GetOrCompile(Path, "mainVS", EShaderType::Vertex, VSCSOPath);
	FShaderByteCode PSCode = RenderUtil::GetOrCompile(Path, "mainPS", EShaderType::Pixel, PSCSOPath);

	if (!VSCode.IsValid() || !PSCode.IsValid())
	{
		HTR_LOG(Error, "[Shader] compile failed: {}", Path);
		return;
	}

	TUniquePtr<FVertexShader> Vs = RenderCommand::CreateVertexShader(VSCode);
	TUniquePtr<FPixelShader>  Ps = RenderCommand::CreatePixelShader(PSCode);

	if (!Vs || !Vs->IsValid() || !Ps || !Ps->IsValid())
	{
		HTR_LOG(Error, "[Shader] device create failed: {}", Path);
		return;
	}

	FVertexShader* VsRaw = Vs.get();
	FPixelShader* PsRaw = Ps.get();

	VertexShaderMap[VSCSOPath] = std::move(Vs);
	PixelShaderMap[PSCSOPath] = std::move(Ps);
	ShaderProgramMap[Path] = MakeUnique<FShaderProgram>(VsRaw, PsRaw);
	ShaderProgramMap[Path]->Name = Path;

	HTR_LOG(Info, "[Shader] loaded: {}", Path);

}

// UberLit 조명 변형처럼 매크로로 VS를 컴파일하고 이름으로 등록한다. PS는 기존 것을 함께 쓴다
void FRenderResourceManager::CreateSkinnedVariant(const FString& Path, const char* LightingMacro, const FShaderProgram& Base)
{
	CreateVertexShaderVariant(Path, LightingMacro, Base, "GPU_SKIN", "#GPU_SKIN");
}

void FRenderResourceManager::CreateVertexShaderVariant(const FString& Path, const char* LightingMacro, const FShaderProgram& Base,
	const char* VariantMacro, const char* Suffix)
{
	const D3D_SHADER_MACRO VariantOnly[] = { { VariantMacro, "1" }, { nullptr, nullptr } };
	const D3D_SHADER_MACRO LitVariant[] = { { LightingMacro, "1" }, { VariantMacro, "1" }, { nullptr, nullptr } };

	FString CSOPath;
	FShaderByteCode Code = RenderUtil::GetOrCompile(Path, "mainVS", EShaderType::Vertex, CSOPath, LightingMacro ? LitVariant : VariantOnly);
	if (!Code.IsValid())
	{
		HTR_LOG(Error, "[Shader] {} variant compile failed: {}", VariantMacro, Base.Name);
		return;
	}
	TUniquePtr<FVertexShader> VS = RenderCommand::CreateVertexShader(Code);
	if (!VS || !VS->IsValid())
	{
		HTR_LOG(Error, "[Shader] {} variant create failed: {}", VariantMacro, Base.Name);
		return;
	}

	FVertexShader* VSRaw = VS.get();
	VertexShaderMap[CSOPath] = std::move(VS);
	const FString Key = Base.Name + Suffix;
	ShaderProgramMap[Key] = MakeUnique<FShaderProgram>(VSRaw, Base.PixelShader);
	ShaderProgramMap[Key]->Name = Key;
}

void FRenderResourceManager::CreateVariantsFromSource(const FString& Path, const std::string& Source, const char* LightingMacro, const FShaderProgram& Base)
{
	if (Source.find("GPU_SKIN_VS_INPUT") != std::string::npos)
		CreateVertexShaderVariant(Path, LightingMacro, Base, "GPU_SKIN", "#GPU_SKIN");
	// 파티클 인스턴싱 변형: SV_InstanceID로 t15의 인스턴스 데이터를 읽는 VS (FRenderPacket::InstanceCount > 0일 때 사용)
	if (Source.find("PARTICLE_INSTANCE_VS_INPUT") != std::string::npos)
		CreateVertexShaderVariant(Path, LightingMacro, Base, "PARTICLE_INSTANCE", ParticleInstanceSuffix);
}

FShaderProgram* FRenderResourceManager::FindShaderProgram(const FString& InName)
{
	if (TUniquePtr<FShaderProgram>* Found = Get().ShaderProgramMap.Find(InName))
		return Found->get();
	return nullptr;
}
