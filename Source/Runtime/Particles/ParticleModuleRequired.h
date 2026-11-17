#pragma once

#include "Particles/ParticleModule.h"
#include "Render/Material.h"
#include "ParticleModuleRequired.generated.h"

UENUM()
enum class EParticleScreenAlignment : uint8
{
	FacingCamera,   // 사각형이 항상 카메라 정면을 본다
	Velocity,       // 속도 방향으로 길게 늘어난다
};

UENUM()
enum class EParticleSortMode : uint8
{
	None,            // 생성 순서 그대로 그린다
	DistanceToView,  // 카메라에서 먼 것부터 그린다
};

UCLASS()
class UParticleModuleRequired : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleRequired();

	// 디테일 패널의 머티리얼 슬롯 UI가 UMaterial 타입일 때만 그려지므로 UMaterialInterface 대신 쓴다
	UPROPERTY(EditAnywhere)
	UMaterial* Material = nullptr;

	UPROPERTY(EditAnywhere)
	EParticleScreenAlignment ScreenAlignment = EParticleScreenAlignment::FacingCamera;

	UPROPERTY(EditAnywhere)
	EParticleSortMode SortMode = EParticleSortMode::DistanceToView;
};
