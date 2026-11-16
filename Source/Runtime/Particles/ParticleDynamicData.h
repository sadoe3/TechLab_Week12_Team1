#pragma once

#include "Core/Types.h"
#include "Math/Vector.h"
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
	// Step 3에서 FParticleSpriteVertex를 만들면 sizeof로 바꾼다
	virtual int32 GetDynamicVertexStride() const override { return 0; }
};
