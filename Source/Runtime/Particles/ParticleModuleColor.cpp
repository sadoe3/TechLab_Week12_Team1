#include "EnginePCH.h"
#include "Particles/ParticleModuleColor.h"
#include "Particles/BaseParticle.h"

UParticleModuleColor::UParticleModuleColor()
{
	bSpawnModule = true;
}

void UParticleModuleColor::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	const FVector Rgb = RandomRange(Owner, StartColorMin, StartColorMax);
	const float Alpha = RandomRange(Owner, StartAlphaMin, StartAlphaMax);
	const FVector4 Color(Rgb.X, Rgb.Y, Rgb.Z, Alpha);
	ParticleBase->Color = Color;
	ParticleBase->BaseColor = Color;
}
