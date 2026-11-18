#include "EnginePCH.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/BaseParticle.h"
#include "Particles/ParticleModuleTypeDataBase.h"
#include "Particles/ParticleSpriteEmitterInstance.h"

UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 LODLevel) const
{
	if (LODLevel < 0 || LODLevel >= LODLevels.Num())
	{
		return nullptr;
	}
	return LODLevels[LODLevel];
}

void UParticleEmitter::AddLODLevel(UParticleLODLevel* InLODLevel)
{
	if (InLODLevel)
	{
		LODLevels.Add(InLODLevel);
	}
}

void UParticleEmitter::ClearLODLevels()
{
	LODLevels.Empty();
}

FParticleEmitterInstance* UParticleEmitter::CreateInstance(UParticleSystemComponent* InComponent)
{
	UParticleLODLevel* LOD0 = GetLODLevel(0);
	if (LOD0 && LOD0->TypeDataModule)
	{
		// Mesh 등 특수 이미터는 TypeData가 자기 인스턴스 타입을 결정한다
		return LOD0->TypeDataModule->CreateInstance(this, InComponent);
	}

	// 언리얼 규칙: TypeDataModule == nullptr이면 Sprite 이미터
	return new FParticleSpriteEmitterInstance(this, InComponent);
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (LODLevel)
		{
			LODLevel->UpdateModuleLists();
		}
	}

	ModuleOffsetMap.Reset();
	TypeDataOffset = 0;
	int32 TotalBytes = sizeof(FBaseParticle);

	// LOD마다 오프셋이 다르면 LOD 전환 때 파티클 메모리를 다시 짜야 하므로 LOD0 기준으로 한 번만 배정한다
	UParticleLODLevel* LOD0 = GetLODLevel(0);
	if (LOD0)
	{
		// UE와 같이 TypeData Payload를 FBaseParticle 바로 뒤에 먼저 둔다
		if (LOD0->TypeDataModule)
		{
			const int32 TypeDataBytes = static_cast<int32>(LOD0->TypeDataModule->RequiredBytes(LOD0->TypeDataModule));
			if (TypeDataBytes > 0)
			{
				TypeDataOffset = TotalBytes;
				TotalBytes += TypeDataBytes;
			}
		}

		for (UParticleModule* Module : LOD0->GetModules())
		{
			if (!Module)
			{
				continue;
			}
			ModuleOffsetMap.FindOrAdd(Module) = TotalBytes;
			TotalBytes += static_cast<int32>(Module->RequiredBytes(LOD0->TypeDataModule));
		}

		// LOD1 이상 모듈은 다른 객체라도 같은 인덱스의 LOD0 자리를 함께 써야 LOD 전환 후에도 Payload가 맞는다
		for (int32 LODIndex = 1; LODIndex < LODLevels.Num(); ++LODIndex)
		{
			UParticleLODLevel* LOD = LODLevels[LODIndex];
			if (!LOD)
			{
				continue;
			}

			const TArray<UParticleModule*>& LOD0Modules = LOD0->GetModules();
			const TArray<UParticleModule*>& Modules = LOD->GetModules();
			const int32 Count = std::min(LOD0Modules.Num(), Modules.Num());
			for (int32 i = 0; i < Count; ++i)
			{
				const int32* Offset = ModuleOffsetMap.Find(LOD0Modules[i]);
				if (Modules[i] && Offset)
				{
					ModuleOffsetMap.FindOrAdd(Modules[i]) = *Offset;
				}
			}
		}
	}
	ParticleSize = TotalBytes;
}

int32 UParticleEmitter::GetModuleOffset(UParticleModule* Module) const
{
	const int32* Offset = ModuleOffsetMap.Find(Module);
	return Offset ? *Offset : 0;
}
