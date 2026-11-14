#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleRequired.generated.h"

class UMaterial;

UENUM()
enum class EParticleScreenAlignment : uint8
{
	FacingCamera,   // 사각형이 항상 카메라 정면을 본다
	Velocity,       // 속도 방향으로 길게 늘어난다
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
};
