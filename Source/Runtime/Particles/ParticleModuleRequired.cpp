#include "EnginePCH.h"
#include "Particles/ParticleModuleRequired.h"
#include "Render/Material.h"
#include "Asset/AssetManager.h"

UParticleModuleRequired::UParticleModuleRequired()
{
	// 렌더러가 LODLevel->RequiredModule로 직접 읽으므로 Spawn/Update 목록에는 넣지 않는다
	bSpawnModule = false;
	bUpdateModule = false;

	// 머티리얼이 비면 프록시가 패킷을 내지 않으므로 기본 스프라이트 머티리얼을 넣어 둔다 (BillboardComponent와 같은 방식)
	Material = UAssetManager::GetAssetByPath<UMaterial>("ParticleSpriteMaterial");
}
