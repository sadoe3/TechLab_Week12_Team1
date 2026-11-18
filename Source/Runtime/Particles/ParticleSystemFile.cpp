#include "EnginePCH.h"
#include "Particles/ParticleSystemFile.h"

#include "Asset/AssetManager.h"
#include "Asset/AssetUtils.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSpawn.h"
#include "Particles/ParticleModuleTypeDataMesh.h"
#include "Particles/ParticleSystem.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/StructuredArchiveSlots.h"
#include "Serialization/MathSerialization.h"
#include "Serialization/Formatter/JsonArchiveOutputFormatter.h"
#include "Serialization/Formatter/JsonArchiveInputFormatter.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <functional>

namespace
{
	// SortMode는 파일에 항목 이름(PSORTMODE_ 뺀 부분)으로 저장한다. 예: "DistanceToView"
	struct FSortModeName
	{
		EParticleSortMode Mode;
		const char* Name;
	};
	constexpr FSortModeName SortModeNames[] = {
		{ EParticleSortMode::PSORTMODE_None, "None" },
		{ EParticleSortMode::PSORTMODE_ViewProjDepth, "ViewProjDepth" },
		{ EParticleSortMode::PSORTMODE_DistanceToView, "DistanceToView" },
		{ EParticleSortMode::PSORTMODE_Age_OldestFirst, "Age_OldestFirst" },
		{ EParticleSortMode::PSORTMODE_Age_NewestFirst, "Age_NewestFirst" },
	};

	const char* SortModeToString(EParticleSortMode Mode)
	{
		for (const FSortModeName& Entry : SortModeNames)
		{
			if (Entry.Mode == Mode) return Entry.Name;
		}
		return "None";
	}

	// 문자열(이름) 또는 숫자 모두 받는다. 키가 없거나 모르는 값이면 None.
	EParticleSortMode SortModeFromJson(const json& Value)
	{
		if (Value.is_string())
		{
			const std::string Name = Value.get<std::string>();
			for (const FSortModeName& Entry : SortModeNames)
			{
				if (Name == Entry.Name || Name == std::string("PSORTMODE_") + Entry.Name) return Entry.Mode;
			}
		}
		else if (Value.is_number_integer())
		{
			const int32 Index = Value.get<int32>();
			if (Index >= 0 && Index <= static_cast<int32>(EParticleSortMode::PSORTMODE_Age_NewestFirst))
				return static_cast<EParticleSortMode>(Index);
		}
		return EParticleSortMode::PSORTMODE_None;
	}

	// .particle은 nlohmann json으로 직접 쓰지만, 엔진 타입(리플렉션 프로퍼티, 머티리얼 참조)은
	// 엔진의 FStructuredArchive 직렬화 코드를 그대로 재사용하기 위해 json 조각 <-> 구조화 아카이브를 왕복시킨다.
	json WriteStructured(const std::function<void(FStructuredArchive::FSlot)>& WriteFunc)
	{
		TArray<uint8> Bytes;
		{
			// 선언 순서 = 소멸 역순: Ar(Close) → Formatter(dump → Writer)
			FMemoryWriter Writer(Bytes);
			Writer.SetIsPersistent(true);
			FJsonArchiveOutputFormatter Formatter(Writer);
			FStructuredArchive Ar(Formatter);
			WriteFunc(Ar.Open());
			Ar.Close();
		}
		return json::parse(Bytes.GetData(), Bytes.GetData() + Bytes.Num(), nullptr, false);
	}

	bool ReadStructured(const json& Json, const std::function<void(FStructuredArchive::FSlot)>& ReadFunc)
	{
		const FString Text = Json.dump();
		TArray<uint8> Bytes;
		Bytes.SetNum(static_cast<int32>(Text.size()));
		std::memcpy(Bytes.GetData(), Text.data(), Text.size());

		FMemoryReader Reader(Bytes, true);
		FJsonArchiveInputFormatter Formatter(Reader);
		if (Reader.IsError())
		{
			return false;
		}

		FStructuredArchive Ar(Formatter);
		ReadFunc(Ar.Open());
		Ar.Close();
		return !Reader.IsError();
	}

	// LODLevel의 일반 모듈(Lifetime, Velocity, ...)은 리플렉션(UPROPERTY)으로 저장한다.
	// { "Class": "UParticleModuleVelocity", "Properties": { ... } }
	// 새 모듈 클래스를 추가해도 이 파일을 고칠 필요가 없다.
	json SaveModuleToJson(UParticleModule* Module)
	{
		return WriteStructured([Module](FStructuredArchive::FSlot Slot)
		{
			FStructuredArchive::FRecord Record = Slot.EnterRecord();
			FString ClassName = Module->GetClass()->Name;
			Record << SA_VALUE("Class", ClassName);
			Module->Serialize(Record);
		});
	}

	UParticleModule* LoadModuleFromJson(const json& ModuleJson)
	{
		UParticleModule* Module = nullptr;
		ReadStructured(ModuleJson, [&Module](FStructuredArchive::FSlot Slot)
		{
			FStructuredArchive::FRecord Record = Slot.EnterRecord();
			FString ClassName;
			Record << SA_VALUE("Class", ClassName);

			UClass* Class = FindClass(ClassName);
			if (!Class || !Class->IsChildOf(UParticleModule::StaticClass()) || !Class->Constructor)
			{
				HTR_LOG(Warning, "ParticleSystem: unknown module class '{}'", ClassName);
				return;
			}

			Module = NewObject<UParticleModule>(nullptr, Class);
			Module->Serialize(Record);
		});
		return Module;
	}

	// 머티리얼 참조는 씬의 메시 머티리얼과 같은 형식을 쓴다 (UMaterial::SerializeMaterialReference)
	//   에셋:      { "Asset": "SubUVMaterial" }
	//   인스턴스:  { "Base": "SubUVMaterial", "Instance": { "Textures": ["SubUV/CloudAtlas.png"], ... } }
	//   없음:      {}
	json SaveMaterialToJson(UMaterial* Material)
	{
		return WriteStructured([Material](FStructuredArchive::FSlot Slot) mutable
		{
			UMaterial::SerializeMaterialReference(Slot, Material);
		});
	}

	UMaterial* LoadMaterialFromJson(const json& MaterialJson)
	{
		UMaterial* Material = nullptr;
		ReadStructured(MaterialJson, [&Material](FStructuredArchive::FSlot Slot)
		{
			UMaterial::SerializeMaterialReference(Slot, Material);
		});
		return Material;
	}
}

bool ParticleSystemFile::Read(const std::filesystem::path& Path, UParticleSystem& OutSystem, FString& OutError)
{
	OutError.clear();

	std::ifstream File(Path);
	if (!File.is_open())
	{
		OutError = "ParticleSystem: cannot open file";
		return false;
	}

	json Root = json::parse(File, nullptr, false);
	if (Root.is_discarded() || !Root.is_object())
	{
		OutError = "ParticleSystem: invalid JSON";
		return false;
	}

	OutSystem.ClearEmitters();
	OutSystem.UpdateTime_FPS = Root.value("UpdateTime_FPS", 60.0f);
	OutSystem.UpdateTime_Delta = Root.value("UpdateTime_Delta", 1.0f / 60.0f);
	OutSystem.WarmupTime = Root.value("WarmupTime", 0.0f);
	OutSystem.WarmupTickRate = Root.value("WarmupTickRate", 0.0f);
	OutSystem.bUseFixedRelativeBoundingBox = Root.value("bUseFixedRelativeBoundingBox", false);
	OutSystem.bOrientZAxisTowardCamera = Root.value("bOrientZAxisTowardCamera", false);

	if (Root.contains("Emitters") && Root["Emitters"].is_array())
	{
		for (const json& EmitterJson : Root["Emitters"])
		{
			if (!EmitterJson.is_object())
			{
				continue;
			}

			UParticleEmitter* Emitter = NewObject<UParticleEmitter>();
			Emitter->SetEmitterName(EmitterJson.value("EmitterName", FString("Particle Emitter")));
			Emitter->SetInitialAllocationCount(EmitterJson.value("InitialAllocationCount", 0));
			Emitter->SetDisabled(EmitterJson.value("bDisabled", false));
			Emitter->bIsSoloing = EmitterJson.value("bIsSoloing", false);

			if (EmitterJson.contains("LODLevels") && EmitterJson["LODLevels"].is_array())
			{
				for (const json& LODJson : EmitterJson["LODLevels"])
				{
					if (!LODJson.is_object())
					{
						continue;
					}

					UParticleLODLevel* LOD = NewObject<UParticleLODLevel>();
					LOD->Level = LODJson.value("Level", 0);
					LOD->bEnabled = LODJson.value("bEnabled", true);
					LOD->PeakActiveParticles = LODJson.value("PeakActiveParticles", 0);

					if (LODJson.contains("RequiredModule") && LODJson["RequiredModule"].is_object())
					{
						const json& ReqJson = LODJson["RequiredModule"];
						UParticleModuleRequired* Req = NewObject<UParticleModuleRequired>();
						Req->bUseLocalSpace = ReqJson.value("bUseLocalSpace", false);
						Req->SortMode = ReqJson.contains("SortMode")
							? SortModeFromJson(ReqJson["SortMode"])
							: EParticleSortMode::PSORTMODE_None;
						Req->bKillOnDeactivate = ReqJson.value("bKillOnDeactivate", false);
						Req->bKillOnCompleted = ReqJson.value("bKillOnCompleted", false);
						Req->EmitterDuration = ReqJson.value("EmitterDuration", 1.0f);
						Req->EmitterDelay = ReqJson.value("EmitterDelay", 0.0f);
						Req->EmitterLoops = ReqJson.value("EmitterLoops", 0);
						Req->MaxDrawCount = ReqJson.value("MaxDrawCount", 500);
						Req->SubImages_Horizontal = ReqJson.value("SubImages_Horizontal", 1);
						Req->SubImages_Vertical = ReqJson.value("SubImages_Vertical", 1);
						if (ReqJson.contains("Material") && ReqJson["Material"].is_object())
						{
							Req->Material = LoadMaterialFromJson(ReqJson["Material"]);
						}
						LOD->RequiredModule = Req;
					}

					if (LODJson.contains("SpawnModule") && LODJson["SpawnModule"].is_object())
					{
						const json& SpawnJson = LODJson["SpawnModule"];
						UParticleModuleSpawn* Spawn = NewObject<UParticleModuleSpawn>();
						Spawn->Rate = SpawnJson.value("Rate", 10.0f);
						Spawn->RateScale = SpawnJson.value("RateScale", 1.0f);
						Spawn->bProcessSpawnRate = SpawnJson.value("bProcessSpawnRate", true);
						Spawn->bProcessBurstList = SpawnJson.value("bProcessBurstList", true);

						if (SpawnJson.contains("BurstList") && SpawnJson["BurstList"].is_array())
						{
							for (const json& BurstJson : SpawnJson["BurstList"])
							{
								if (!BurstJson.is_object())
								{
									continue;
								}
								FParticleBurst Burst;
								Burst.Count = BurstJson.value("Count", 0);
								Burst.CountLow = BurstJson.value("CountLow", -1);
								Burst.Time = BurstJson.value("Time", 0.0f);
								Spawn->AddBurst(Burst);
							}
						}

						LOD->SpawnModule = Spawn;
					}

					if (LODJson.contains("TypeDataModule") && LODJson["TypeDataModule"].is_object())
					{
						const json& TypeJson = LODJson["TypeDataModule"];
						const FString TypeName = TypeJson.value("Type", FString(""));

						if (TypeName == "Mesh")
						{
							UParticleModuleTypeDataMesh* MeshType = NewObject<UParticleModuleTypeDataMesh>();
							const FString MeshPath = TypeJson.value("MeshPath", FString(""));
							if (!MeshPath.empty())
							{
								MeshType->SetMesh(UAssetManager::GetAssetByPath<UStaticMesh>(MeshPath));
							}
							MeshType->LODSizeScale = TypeJson.value("LODSizeScale", 1.0f);
							MeshType->bUseStaticMeshLODs = TypeJson.value("bUseStaticMeshLODs", true);
							MeshType->bCastShadows = TypeJson.value("bCastShadows", false);
							MeshType->bOverrideMaterial = TypeJson.value("bOverrideMaterial", false);
							LOD->TypeDataModule = MeshType;
						}
						else if (TypeName == "Sprite")
						{
							// 구버전 에셋 호환: Sprite는 TypeData 없이(nullptr) 표현하고, SubUV 값은 RequiredModule로 옮긴다
							const bool bReqHasSubUV = LODJson.contains("RequiredModule")
								&& LODJson["RequiredModule"].is_object()
								&& LODJson["RequiredModule"].contains("SubImages_Horizontal");
							if (LOD->RequiredModule && !bReqHasSubUV)
							{
								LOD->RequiredModule->SubImages_Horizontal = TypeJson.value("SubImages_Horizontal", 1);
								LOD->RequiredModule->SubImages_Vertical = TypeJson.value("SubImages_Vertical", 1);
							}
						}
					}

					if (LODJson.contains("Modules") && LODJson["Modules"].is_array())
					{
						for (const json& ModuleJson : LODJson["Modules"])
						{
							if (!ModuleJson.is_object())
							{
								continue;
							}
							if (UParticleModule* Module = LoadModuleFromJson(ModuleJson))
							{
								LOD->AddModule(Module);
							}
						}
					}

					Emitter->AddLODLevel(LOD);
				}
			}

			OutSystem.AddEmitter(Emitter);
		}
	}

	OutSystem.UpdateAllModuleLists();
	OutSystem.SetPath(FAssetUtils::GetAssetKey(Path.generic_string()));
	return true;
}

bool ParticleSystemFile::Write(const std::filesystem::path& Path, UParticleSystem& System, FString& OutError)
{
	OutError.clear();

	json Root = json::object();
	Root["UpdateTime_FPS"] = System.UpdateTime_FPS;
	Root["UpdateTime_Delta"] = System.UpdateTime_Delta;
	Root["WarmupTime"] = System.WarmupTime;
	Root["WarmupTickRate"] = System.WarmupTickRate;
	Root["bUseFixedRelativeBoundingBox"] = System.bUseFixedRelativeBoundingBox;
	Root["bOrientZAxisTowardCamera"] = System.bOrientZAxisTowardCamera;

	json EmittersJson = json::array();
	for (const UParticleEmitter* Emitter : System.GetEmitters())
	{
		if (!Emitter)
		{
			continue;
		}

		json EmitterJson = json::object();
		EmitterJson["EmitterName"] = Emitter->GetEmitterName();
		EmitterJson["InitialAllocationCount"] = Emitter->GetInitialAllocationCount();
		EmitterJson["bDisabled"] = Emitter->IsDisabled();
		EmitterJson["bIsSoloing"] = Emitter->bIsSoloing;

		json LODsJson = json::array();
		for (const UParticleLODLevel* LOD : Emitter->GetLODLevels())
		{
			if (!LOD)
			{
				continue;
			}

			json LODJson = json::object();
			LODJson["Level"] = LOD->Level;
			LODJson["bEnabled"] = LOD->bEnabled;
			LODJson["PeakActiveParticles"] = LOD->PeakActiveParticles;

			if (LOD->RequiredModule)
			{
				const UParticleModuleRequired* Req = LOD->RequiredModule;
				json ReqJson = json::object();
				ReqJson["bUseLocalSpace"] = Req->bUseLocalSpace;
				ReqJson["SortMode"] = SortModeToString(Req->SortMode);
				ReqJson["bKillOnDeactivate"] = Req->bKillOnDeactivate;
				ReqJson["bKillOnCompleted"] = Req->bKillOnCompleted;
				ReqJson["EmitterDuration"] = Req->EmitterDuration;
				ReqJson["EmitterDelay"] = Req->EmitterDelay;
				ReqJson["EmitterLoops"] = Req->EmitterLoops;
				ReqJson["MaxDrawCount"] = Req->MaxDrawCount;
				ReqJson["SubImages_Horizontal"] = Req->SubImages_Horizontal;
				ReqJson["SubImages_Vertical"] = Req->SubImages_Vertical;
				const json MaterialJson = SaveMaterialToJson(Req->Material);
				if (!MaterialJson.is_discarded())
				{
					ReqJson["Material"] = MaterialJson;
				}
				LODJson["RequiredModule"] = ReqJson;
			}

			if (LOD->SpawnModule)
			{
				const UParticleModuleSpawn* Spawn = LOD->SpawnModule;
				json SpawnJson = json::object();
				SpawnJson["Rate"] = Spawn->Rate;
				SpawnJson["RateScale"] = Spawn->RateScale;
				SpawnJson["bProcessSpawnRate"] = Spawn->bProcessSpawnRate;
				SpawnJson["bProcessBurstList"] = Spawn->bProcessBurstList;

				json BurstListJson = json::array();
				for (const FParticleBurst& Burst : Spawn->GetBurstList())
				{
					json BurstJson = json::object();
					BurstJson["Count"] = Burst.Count;
					BurstJson["CountLow"] = Burst.CountLow;
					BurstJson["Time"] = Burst.Time;
					BurstListJson.push_back(BurstJson);
				}
				SpawnJson["BurstList"] = BurstListJson;
				LODJson["SpawnModule"] = SpawnJson;
			}

			if (LOD->TypeDataModule)
			{
				if (const UParticleModuleTypeDataMesh* MeshType = Cast<UParticleModuleTypeDataMesh>(LOD->TypeDataModule))
				{
					json TypeJson = json::object();
					TypeJson["Type"] = "Mesh";
					TypeJson["MeshPath"] = MeshType->GetMesh() ? MeshType->GetMesh()->GetPath() : FString("");
					TypeJson["LODSizeScale"] = MeshType->LODSizeScale;
					TypeJson["bUseStaticMeshLODs"] = MeshType->bUseStaticMeshLODs;
					TypeJson["bCastShadows"] = MeshType->bCastShadows;
					TypeJson["bOverrideMaterial"] = MeshType->bOverrideMaterial;
					LODJson["TypeDataModule"] = TypeJson;
				}
			}

			json ModulesJson = json::array();
			for (UParticleModule* Module : LOD->GetModules())
			{
				if (!Module)
				{
					continue;
				}
				json ModuleJson = SaveModuleToJson(Module);
				if (!ModuleJson.is_discarded())
				{
					ModulesJson.push_back(ModuleJson);
				}
			}
			LODJson["Modules"] = ModulesJson;

			LODsJson.push_back(LODJson);
		}

		EmitterJson["LODLevels"] = LODsJson;
		EmittersJson.push_back(EmitterJson);
	}
	Root["Emitters"] = EmittersJson;

	if (Path.has_parent_path())
	{
		std::error_code EC;
		std::filesystem::create_directories(Path.parent_path(), EC);
	}

	std::ofstream File(Path, std::ios::binary | std::ios::trunc);
	if (!File.is_open())
	{
		OutError = "ParticleSystem: cannot open output file";
		return false;
	}

	const FString Serialized = Root.dump(2);
	File.write(Serialized.data(), static_cast<std::streamsize>(Serialized.size()));
	File.close();
	if (!File)
	{
		OutError = "ParticleSystem: cannot write file";
		return false;
	}

	System.SetPath(FAssetUtils::GetAssetKey(Path.generic_string()));
	return true;
}

bool ParticleSystemFile::IsParticleSystemPath(const FString& Path)
{
	FString Extension = std::filesystem::path(Path).extension().string();

	std::transform(Extension.begin(), Extension.end(), Extension.begin(),
		[](unsigned char Character) { return static_cast<char>(std::tolower(Character)); });

	return Extension == ".particle";
}
