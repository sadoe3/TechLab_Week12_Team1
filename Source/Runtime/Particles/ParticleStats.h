#pragma once
#include "Core/Stats/LightweightStats.h"

// "stat particle"에서 보여 줄 파티클 CPU 구간 (Unreal STATGROUP_Particles 대응)
namespace ParticleStats
{
	DECLARE_CYCLE_STAT("Particle: Tick", STAT_ParticleTick);
	DECLARE_CYCLE_STAT("Particle: Collision", STAT_ParticleCollision);
	DECLARE_CYCLE_STAT("Particle: Dynamic Data", STAT_ParticleDynamicData);
	DECLARE_CYCLE_STAT("Particle: Render Gather", STAT_ParticleRenderGather);
}
