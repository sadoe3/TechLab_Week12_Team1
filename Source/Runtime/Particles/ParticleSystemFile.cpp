#include "EnginePCH.h"
#include "Particles/ParticleSystemFile.h"

#include "Asset/AssetManager.h"
#include "Asset/AssetUtils.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSpawn.h"
#include "Particles/ParticleModuleTypeDataMesh.h"
#include "Particles/ParticleModuleTypeDataRibbon.h"
#include "Particles/ParticleModuleTypeDataBeam2.h"
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

	// RenderAxis도 Trails_ 뺀 이름으로 저장한다. 배열 순서 = enum 값
	constexpr const char* RenderAxisNames[] = { "CameraUp", "SourceUp", "WorldUp" };

	ETrailsRenderAxisOption RenderAxisFromString(const std::string& Name)
	{
		for (int32 i = 0; i < 3; ++i)
		{
			if (Name == RenderAxisNames[i])
			{
				return static_cast<ETrailsRenderAxisOption>(i);
			}
		}
		return ETrailsRenderAxisOption::Trails_CameraUp;
	}

	// Beam enum도 접두사 뺀 이름으로 저장한다. 배열 순서 = enum 값
	constexpr const char* BeamMethodNames[] = { "Distance", "Target" };
	constexpr const char* BeamTaperMethodNames[] = { "None", "Full" };

	template <typename EnumType, size_t N>
	EnumType EnumFromName(const char* const (&Names)[N], const std::string& Name, EnumType Default)
	{
		for (size_t i = 0; i < N; ++i)
		{
			if (Name == Names[i])
			{
				return static_cast<EnumType>(i);
			}
		}
		return Default;
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

	// Files saved before curves stored over-life values as a start/end pair that was lerped over the particle's life.
	// Curve property name -> the old pair's keys.
	struct FLegacyCurvePair
	{
		const char* CurveName;
		const char* StartName;
		const char* EndName;
	};
	constexpr FLegacyCurvePair LegacyCurvePairs[] = {
		{ "ColorOverLife",  "ColorStart",      "ColorEnd" },       // UParticleModuleColorOverLife
		{ "AlphaOverLife",  "AlphaStart",      "AlphaEnd" },       // UParticleModuleColorOverLife
		{ "LifeMultiplier", "MultiplierStart", "MultiplierEnd" },  // UParticleModuleSizeMultiplyLife, UParticleModuleRotationRateMultiplyLife
	};

	// Zero with the same shape as Value: 0.0 for a float, { "X": 0, "Y": 0, "Z": 0 } for a vector
	json ZeroLike(const json& Value)
	{
		if (!Value.is_object())
		{
			return 0.0f;
		}
		json Zero = json::object();
		for (auto It = Value.begin(); It != Value.end(); ++It)
		{
			Zero[It.key()] = 0.0f;
		}
		return Zero;
	}

	// Turns an old start/end pair into a two-key linear curve (RelativeTime 0 -> 1), which evaluates to the same lerp.
	void UpgradeStartEndToCurve(const FString& CurveName, json& Properties)
	{
		for (const FLegacyCurvePair& Pair : LegacyCurvePairs)
		{
			if (CurveName != Pair.CurveName)
			{
				continue;
			}
			if (Properties.contains(CurveName) || !Properties.contains(Pair.StartName) || !Properties.contains(Pair.EndName))
			{
				return;
			}

			const json& Start = Properties[Pair.StartName];
			const json& End = Properties[Pair.EndName];
			const auto MakePoint = [](float InVal, const json& OutVal)
			{
				json Point = json::object();
				Point["InVal"] = InVal;
				Point["OutVal"] = OutVal;
				Point["ArriveTangent"] = ZeroLike(OutVal);
				Point["LeaveTangent"] = ZeroLike(OutVal);
				Point["InterpMode"] = "Linear";
				return Point;
			};

			json Curve = json::object();
			Curve["Points"] = json::array({ MakePoint(0.0f, Start), MakePoint(1.0f, End) });
			Properties[CurveName] = Curve;
			Properties.erase(Pair.StartName);
			Properties.erase(Pair.EndName);
			return;
		}
	}

	// Files saved before distributions stored spawn-time values as "XMin" / "XMax" pairs (e.g. LifetimeMin / LifetimeMax).
	// For each distribution property X of the module class that the file lacks, turn a matching pair into
	// { "Type": "Constant" } (Min == Max) or { "Type": "Uniform" } so old assets keep their values.
	// Old over-life start/end pairs become linear curves (UpgradeStartEndToCurve).
	json UpgradeMinMaxToDistributions(const json& ModuleJson)
	{
		if (!ModuleJson.is_object() || !ModuleJson.contains("Class") || !ModuleJson["Class"].is_string()
			|| !ModuleJson.contains("Properties") || !ModuleJson["Properties"].is_object())
		{
			return ModuleJson;
		}
		const UClass* Class = FindClass(ModuleJson["Class"].get<std::string>());
		if (!Class)
		{
			return ModuleJson;
		}

		json Upgraded = ModuleJson;
		json& Properties = Upgraded["Properties"];
		for (const UStruct* Struct = Class; Struct; Struct = Struct->GetSuperStruct())
		{
			for (const FProperty* Property : Struct->GetProperties())
			{
				if (Property->IsA<FInterpCurveFloatProperty>() || Property->IsA<FInterpCurveVectorProperty>())
				{
					UpgradeStartEndToCurve(Property->Name, Properties);
					continue;
				}
				if (!Property->IsA<FDistributionFloatProperty>() && !Property->IsA<FDistributionVectorProperty>())
				{
					continue;
				}
				const FString& Name = Property->Name;
				const FString MinName = Name + "Min";
				const FString MaxName = Name + "Max";
				if (Properties.contains(Name) || !Properties.contains(MinName) || !Properties.contains(MaxName))
				{
					continue;
				}

				json Distribution = json::object();
				if (Properties[MinName] == Properties[MaxName])
				{
					Distribution["Type"] = "Constant";
					Distribution["Constant"] = Properties[MinName];
				}
				else
				{
					Distribution["Type"] = "Uniform";
					Distribution["Min"] = Properties[MinName];
					Distribution["Max"] = Properties[MaxName];
				}
				Properties[Name] = Distribution;
				Properties.erase(MinName);
				Properties.erase(MaxName);
			}
		}
		return Upgraded;
	}

	UParticleModule* LoadModuleFromJson(const json& ModuleJson)
	{
		UParticleModule* Module = nullptr;
		ReadStructured(UpgradeMinMaxToDistributions(ModuleJson), [&Module](FStructuredArchive::FSlot Slot)
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

	// 이미터 하나 <-> json. 파일 읽기/쓰기와 에디터의 이미터 복제가 같은 코드를 쓴다.
	UParticleEmitter* EmitterFromJson(const json& EmitterJson)
	{
		UParticleEmitter* Emitter = NewObject<UParticleEmitter>();
		Emitter->SetEmitterName(EmitterJson.value("EmitterName", FString("Particle Emitter")));
		Emitter->SetInitialAllocationCount(EmitterJson.value("InitialAllocationCount", 0));
		Emitter->SetDisabled(EmitterJson.value("bDisabled", false));
		Emitter->bIsSoloing = EmitterJson.value("bIsSoloing", false);

		if (!EmitterJson.contains("LODLevels") || !EmitterJson["LODLevels"].is_array())
		{
			return Emitter;
		}

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
				// Rate used to be a plain number; now it is a distribution object. Read both.
				if (SpawnJson.contains("Rate") && SpawnJson["Rate"].is_number())
				{
					Spawn->Rate = FDistributionFloat::MakeConstant(SpawnJson["Rate"].get<float>());
				}
				else if (SpawnJson.contains("Rate") && SpawnJson["Rate"].is_object())
				{
					ReadStructured(SpawnJson["Rate"], [Spawn](FStructuredArchive::FSlot Slot) { Slot << Spawn->Rate; });
				}
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
				else if (TypeName == "Ribbon")
				{
					UParticleModuleTypeDataRibbon* RibbonType = NewObject<UParticleModuleTypeDataRibbon>();
					RibbonType->MaxParticleInTrailCount = TypeJson.value("MaxParticleInTrailCount", 0);
					RibbonType->TilingDistance = TypeJson.value("TilingDistance", 0.0f);
					RibbonType->RenderAxis = RenderAxisFromString(TypeJson.value("RenderAxis", std::string("CameraUp")));
					RibbonType->bClipSourceSegement = TypeJson.value("bClipSourceSegement", false);
					RibbonType->bDeadTrailsOnDeactivate = TypeJson.value("bDeadTrailsOnDeactivate", false);
					LOD->TypeDataModule = RibbonType;
				}
				else if (TypeName == "Beam2")
				{
					UParticleModuleTypeDataBeam2* BeamType = NewObject<UParticleModuleTypeDataBeam2>();
					BeamType->Segments = TypeJson.value("Segments", 10);
					BeamType->BeamMethod = EnumFromName(BeamMethodNames, TypeJson.value("BeamMethod", std::string("Target")), EBeam2Method::PEBM2_Target);
					BeamType->Distance = TypeJson.value("Distance", 5.0f);
					BeamType->TextureTileDistance = TypeJson.value("TextureTileDistance", 0.0f);
					BeamType->TaperMethod = EnumFromName(BeamTaperMethodNames, TypeJson.value("TaperMethod", std::string("None")), EBeamTaperMethod::PEBTM_None);
					BeamType->TaperFactor = TypeJson.value("TaperFactor", 1.0f);
					LOD->TypeDataModule = BeamType;
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

		return Emitter;
	}

	json EmitterToJson(const UParticleEmitter& Emitter)
	{
		json EmitterJson = json::object();
		EmitterJson["EmitterName"] = Emitter.GetEmitterName();
		EmitterJson["InitialAllocationCount"] = Emitter.GetInitialAllocationCount();
		EmitterJson["bDisabled"] = Emitter.IsDisabled();
		EmitterJson["bIsSoloing"] = Emitter.bIsSoloing;

		json LODsJson = json::array();
		for (const UParticleLODLevel* LOD : Emitter.GetLODLevels())
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
				FDistributionFloat Rate = Spawn->Rate;
				SpawnJson["Rate"] = WriteStructured([&Rate](FStructuredArchive::FSlot Slot) { Slot << Rate; });
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
				else if (UParticleModuleTypeDataRibbon* RibbonType = Cast<UParticleModuleTypeDataRibbon>(LOD->TypeDataModule))
				{
					json TypeJson = json::object();
					TypeJson["Type"] = "Ribbon";
					TypeJson["MaxParticleInTrailCount"] = RibbonType->MaxParticleInTrailCount;
					TypeJson["TilingDistance"] = RibbonType->TilingDistance;
					TypeJson["RenderAxis"] = RenderAxisNames[static_cast<int32>(RibbonType->RenderAxis)];
					TypeJson["bClipSourceSegement"] = RibbonType->bClipSourceSegement;
					TypeJson["bDeadTrailsOnDeactivate"] = RibbonType->bDeadTrailsOnDeactivate;
					LODJson["TypeDataModule"] = TypeJson;
				}
				else if (const UParticleModuleTypeDataBeam2* BeamType = Cast<UParticleModuleTypeDataBeam2>(LOD->TypeDataModule))
				{
					json TypeJson = json::object();
					TypeJson["Type"] = "Beam2";
					TypeJson["Segments"] = BeamType->Segments;
					TypeJson["BeamMethod"] = BeamMethodNames[static_cast<int32>(BeamType->BeamMethod)];
					TypeJson["Distance"] = BeamType->Distance;
					TypeJson["TextureTileDistance"] = BeamType->TextureTileDistance;
					TypeJson["TaperMethod"] = BeamTaperMethodNames[static_cast<int32>(BeamType->TaperMethod)];
					TypeJson["TaperFactor"] = BeamType->TaperFactor;
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
		return EmitterJson;
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
	OutSystem.LODMethod = static_cast<EParticleSystemLODMethod>(Root.value("LODMethod", 0));
	OutSystem.LODDistances = { 0.0f };
	if (Root.contains("LODDistances") && Root["LODDistances"].is_array())
	{
		OutSystem.LODDistances.Empty();
		for (const json& Distance : Root["LODDistances"])
		{
			OutSystem.LODDistances.Add(Distance.get<float>());
		}
	}

	if (Root.contains("Emitters") && Root["Emitters"].is_array())
	{
		for (const json& EmitterJson : Root["Emitters"])
		{
			if (EmitterJson.is_object())
			{
				OutSystem.AddEmitter(EmitterFromJson(EmitterJson));
			}
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
	Root["LODMethod"] = static_cast<int32>(System.LODMethod);
	json DistancesJson = json::array();
	for (float Distance : System.LODDistances)
	{
		DistancesJson.push_back(Distance);
	}
	Root["LODDistances"] = DistancesJson;

	json EmittersJson = json::array();
	for (const UParticleEmitter* Emitter : System.GetEmitters())
	{
		if (Emitter)
		{
			EmittersJson.push_back(EmitterToJson(*Emitter));
		}
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

UParticleEmitter* ParticleSystemFile::DuplicateEmitter(const UParticleEmitter& Source)
{
	UParticleEmitter* Copy = EmitterFromJson(EmitterToJson(Source));
	Copy->CacheEmitterModuleInfo();
	return Copy;
}

UParticleLODLevel* ParticleSystemFile::DuplicateLODLevel(const UParticleEmitter& Source, int32 LODIndex)
{
	// LOD 하나만 담은 이미터 json을 만들어 읽으면, 이미터 읽기 코드를 그대로 써서 LOD만 꺼낼 수 있다
	json EmitterJson = EmitterToJson(Source);
	json OnlyLOD = json::object();
	OnlyLOD["LODLevels"] = json::array({ EmitterJson["LODLevels"][LODIndex] });

	UParticleEmitter* Temp = EmitterFromJson(OnlyLOD);
	return Temp->GetLODLevel(0);
}

bool ParticleSystemFile::IsParticleSystemPath(const FString& Path)
{
	FString Extension = std::filesystem::path(Path).extension().string();

	std::transform(Extension.begin(), Extension.end(), Extension.begin(),
		[](unsigned char Character) { return static_cast<char>(std::tolower(Character)); });

	return Extension == ".particle";
}
