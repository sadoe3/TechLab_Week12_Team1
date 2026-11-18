#pragma once

#include "Core/EngineString.h"

#include <filesystem>

class UParticleSystem;
class UParticleEmitter;

namespace ParticleSystemFile
{
	bool Read(const std::filesystem::path& Path, UParticleSystem& OutSystem, FString& OutError);
	bool Write(const std::filesystem::path& Path, UParticleSystem& System, FString& OutError);

	/** 저장 형식으로 한 번 썼다가 다시 읽어 이미터를 통째로 복제한다. 저장되는 값은 모두 복제된다. */
	UParticleEmitter* DuplicateEmitter(const UParticleEmitter& Source);

	bool IsParticleSystemPath(const FString& Path);
}