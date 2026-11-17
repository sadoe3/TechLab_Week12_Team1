#pragma once

#include "Core/Types.h"
#include "Math/Vector.h"
#include "Render/Vertex.h"
#include "Particles/ParticleHelper.h"
#include <malloc.h>
#include <cstring>

class UMaterial;

enum class EDynamicEmitterType : uint8
{
	Unknown,
	Sprite,
	Mesh,
};

// 인스턴스와 같은 모양(데이터 뒤에 인덱스)으로 자기 블록을 따로 들고 있는 스냅샷 메모리
struct FParticleDataContainer
{
	int32 MemBlockSize = 0;
	int32 ParticleDataNumBytes = 0;
	int32 ParticleIndicesNumShorts = 0;
	uint8* ParticleData = nullptr;      // 블록의 시작. 해제도 이 포인터로 한다
	uint16* ParticleIndices = nullptr;  // 같은 블록의 ParticleData 뒤를 가리킨다

	FParticleDataContainer() = default;
	~FParticleDataContainer() { Free(); }

	// 블록을 직접 들고 있으므로 복사되면 두 번 해제된다
	FParticleDataContainer(const FParticleDataContainer&) = delete;
	FParticleDataContainer& operator=(const FParticleDataContainer&) = delete;

	void Alloc(int32 InParticleDataNumBytes, int32 InParticleIndicesNumShorts)
	{
		Free();
		ParticleDataNumBytes = InParticleDataNumBytes;
		ParticleIndicesNumShorts = InParticleIndicesNumShorts;
		MemBlockSize = ParticleDataNumBytes + ParticleIndicesNumShorts * static_cast<int32>(sizeof(uint16));
		if (MemBlockSize <= 0)
		{
			return;
		}
		ParticleData = static_cast<uint8*>(_aligned_malloc(MemBlockSize, 16));
		ParticleIndices = reinterpret_cast<uint16*>(ParticleData + ParticleDataNumBytes);
	}

	void Free()
	{
		_aligned_free(ParticleData);
		ParticleData = nullptr;
		ParticleIndices = nullptr;
		MemBlockSize = ParticleDataNumBytes = ParticleIndicesNumShorts = 0;
	}
};

// 렌더러가 그리는 데 필요한 재료만 담은 순수 데이터
struct FDynamicEmitterReplayDataBase
{
	EDynamicEmitterType eEmitterType = EDynamicEmitterType::Unknown;
	int32 ActiveParticleCount = 0;
	int32 ParticleStride = 0;
	FParticleDataContainer DataContainer;
	FVector Scale = FVector(1.0f, 1.0f, 1.0f);
	int32 SortMode = 0;

	virtual ~FDynamicEmitterReplayDataBase() = default;
};

struct FDynamicSpriteEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	UMaterial* Material = nullptr;      // 에셋을 가리키기만 하고 해제하지 않는다
};

// 재료(Source)를 어떻게 그릴지 아는 쪽. 렌더러는 이 기본 타입만 본다
struct FDynamicEmitterDataBase
{
	int32 EmitterIndex = 0;

	virtual ~FDynamicEmitterDataBase() = default;
	virtual const FDynamicEmitterReplayDataBase& GetSource() const = 0;
};

struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{
	// Phase 4: SortSpriteParticles
	virtual int32 GetDynamicVertexStride() const = 0;
};

struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
	FDynamicSpriteEmitterReplayDataBase Source;

	virtual const FDynamicEmitterReplayDataBase& GetSource() const override { return Source; }
	virtual int32 GetDynamicVertexStride() const override { return sizeof(FParticleSpriteVertex); }

	// Dst에는 ActiveParticleCount * 4 칸이 있어야 한다
	void FillVertices(FParticleSpriteVertex* Dst) const;
};

// 스냅샷의 살아 있는 파티클마다 정점 4개를 채운다 (UV만 다르게)
inline void FDynamicSpriteEmitterData::FillVertices(FParticleSpriteVertex* Dst) const
{
	static const FVector2 Corners[4] = { FVector2(0.f, 0.f), FVector2(1.f, 0.f), FVector2(1.f, 1.f), FVector2(0.f, 1.f) };

	const FParticleDataContainer& Container = Source.DataContainer;
	for (int32 i = 0; i < Source.ActiveParticleCount; ++i)
	{
		const int32 Index = Container.ParticleIndices[i];
		const FBaseParticle& Particle = *reinterpret_cast<const FBaseParticle*>(Container.ParticleData + Index * Source.ParticleStride);

		FParticleSpriteVertex Vertex;
		Vertex.Position = Particle.Location;
		Vertex.Rotation = Particle.Rotation;
		Vertex.Size = FVector2(Particle.Size.X * Source.Scale.X, Particle.Size.Y * Source.Scale.Y);
		Vertex.Color = Particle.Color;

		for (int32 c = 0; c < 4; ++c)
		{
			Vertex.UV = Corners[c];
			Dst[i * 4 + c] = Vertex;
		}
	}
}
