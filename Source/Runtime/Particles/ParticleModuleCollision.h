#pragma once

#include "ParticleModule.h"
#include "ParticleModuleCollision.generated.h"

class UPrimitiveComponent;

UENUM()
enum class EParticleCollisionResponse : uint8
{
	Bounce = 0,		// 반사
	Stop = 1,		// 충돌 지점에 멈춤
	Kill = 2,		// 즉시 제거
};

struct FParticleCollisionPayload
{
	int32 UsedCollisions = 0;
	// 몇번 콜리전 반응했는지 저장.
	// MaxCollisions 와 비교해서 이 값을 넘으면
	// OnMaxCollisions 에 저장된 Response를 적용시킴
};

UCLASS()
class UParticleModuleCollision : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleCollision();
	virtual ~UParticleModuleCollision() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void FinalUpdate(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) const override;
	
public:
	UPROPERTY(EditAnywhere)
	EParticleCollisionResponse Response = EParticleCollisionResponse::Bounce;

	UPROPERTY(EditAnywhere)
	float Restitution = 0.5f;

	UPROPERTY(EditAnywhere)
	float Friction = 0.2f;

	UPROPERTY(EditAnywhere)
	float RadiusScale = 1.0f;

	UPROPERTY(EditAnywhere)
	int32 MaxCollisions = 0;

	UPROPERTY(EditAnywhere)
	EParticleCollisionResponse OnMaxCollisions = EParticleCollisionResponse::Stop;

	UPROPERTY(EditAnywhere)
	float MinBounceSpeed = 0.5f;

private:
	TArray<UPrimitiveComponent*> Candidates;
	TArray<FBox> CandidateBounds;
};