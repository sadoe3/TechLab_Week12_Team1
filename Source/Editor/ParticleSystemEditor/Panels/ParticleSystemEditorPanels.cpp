#include "EnginePCH.h"
#include "ParticleSystemEditorPanels.h"
#include "Editor/ParticleSystemEditor/ParticleSystemEditor.h"
#include "Editor/Details/PropertyEditor.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSpawn.h"
#include "Particles/ParticleModuleTypeDataBase.h"
#include "Particles/ParticleModuleTypeDataMesh.h"
#include "Particles/ParticleModuleTypeDataRibbon.h"
#include "Particles/ParticleModuleTypeDataBeam2.h"

#include "UObject/UObjectHash.h"

#include <imgui_internal.h>     // BeginDragDropTargetCustom, ImRect
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <map>
#include <vector>

namespace
{
	constexpr const char* PanelIdPrefix = "ParticleSystemPanel";

	// 모듈 드래그 페이로드. 받는 쪽이 같은 이미터인지 보고 순서 변경/이동을 정한다.
	constexpr const char* ModulePayloadType = "PARTICLE_MODULE";
	struct FModuleDragPayload
	{
		UParticleEmitter* Emitter = nullptr;
		UParticleModule* Module = nullptr;
	};

	// Cascade에 보이는 이름과 모듈 추가 메뉴의 분류
	struct FModuleClassInfo
	{
		const char* ClassName;
		const char* DisplayName;
		const char* Category;
	};
	constexpr FModuleClassInfo KnownModules[] = {
		{ "UParticleModuleColor",             "Initial Color",    "Color" },
		{ "UParticleModuleColorOverLife",     "Color Over Life",  "Color" },
		{ "UParticleModuleLifetime",          "Lifetime",         "Lifetime" },
		{ "UParticleModuleLocation",          "Initial Location", "Location" },
		{ "UParticleModuleSize",              "Initial Size",     "Size" },
		{ "UParticleModuleSizeMultiplyLife",  "Size By Life",     "Size" },
		{ "UParticleModuleVelocity",          "Initial Velocity", "Velocity" },
		{ "UParticleModuleRotation",          "Initial Rotation", "Rotation" },
		{ "UParticleModuleRotationRate",      "Initial Rotation Rate", "Rotation Rate" },
		{ "UParticleModuleRotationRateMultiplyLife", "Rotation Rate By Life", "Rotation Rate" },
		{ "UParticleModuleSubUV",             "Sub Image Index",  "SubUV" },
		{ "UParticleModuleTypeDataMesh",      "Mesh Data",        "TypeData" },
		{ "UParticleModuleTypeDataRibbon",    "Ribbon Data",      "TypeData" },
		{ "UParticleModuleTypeDataBeam2",     "Beam Data",        "TypeData" },
		{ "UParticleModuleBeamTarget",        "Beam Target",      "Beam" },
		{ "UParticleModuleBeamNoise",         "Noise",            "Beam" },
	};

	const FModuleClassInfo* FindModuleClassInfo(const UClass* Class)
	{
		for (const FModuleClassInfo& Info : KnownModules)
		{
			if (Class && Class->Name == Info.ClassName)
			{
				return &Info;
			}
		}
		return nullptr;
	}

	// 표에 없으면 접두사만 뗀다. "UParticleModuleFoo" -> "Foo"
	FString GetModuleClassDisplayName(const UClass* Class)
	{
		if (!Class)
		{
			return "None";
		}
		if (const FModuleClassInfo* Info = FindModuleClassInfo(Class))
		{
			return Info->DisplayName;
		}

		FString Name = Class->Name;
		for (const char* Prefix : { "UParticleModule", "ParticleModule" })
		{
			const FString PrefixString(Prefix);
			if (Name.rfind(PrefixString, 0) == 0 && Name.size() > PrefixString.size())
			{
				Name = Name.substr(PrefixString.size());
				break;
			}
		}
		return Name;
	}

	FString GetModuleDisplayName(const UObject* Module)
	{
		return Module ? GetModuleClassDisplayName(Module->GetClass()) : FString("None");
	}

	// 이미터 열에서 같은 패널의 모듈 행 사이에 끼울 자리를 표시한다.
	void DrawDropLine(float MinX, float MaxX, float Y)
	{
		ImGui::GetWindowDrawList()->AddLine(ImVec2(MinX, Y), ImVec2(MaxX, Y), IM_COL32(255, 200, 60, 255), 2.0f);
	}

	// Cascade의 모듈 행 색 (Required: 노랑, Spawn: 빨강, TypeData: 회색, 나머지: 기본)
	ImVec4 GetModuleRowColor(const char* Kind)
	{
		if (std::strcmp(Kind, "Required") == 0) return ImVec4(0.55f, 0.52f, 0.20f, 1.0f);
		if (std::strcmp(Kind, "Spawn") == 0)    return ImVec4(0.60f, 0.25f, 0.25f, 1.0f);
		if (std::strcmp(Kind, "TypeData") == 0) return ImVec4(0.30f, 0.30f, 0.34f, 1.0f);
		return ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	}
}

////////////////////////////////////////////////////////////
// Viewport
////////////////////////////////////////////////////////////

FParticleViewportPanel::FParticleViewportPanel(FParticleSystemEditor& InEditor)
	: FParticleSystemEditorPanel(InEditor, "Viewport", PanelIdPrefix)
{
}

void FParticleViewportPanel::DrawToolbar()
{
	if (ImGui::Button("Restart Sim"))
	{
		Editor.RestartSimulation();
	}
	ImGui::SameLine();
	if (ImGui::Button(Editor.IsPaused() ? "Play" : "Pause"))
	{
		Editor.SetPaused(!Editor.IsPaused());
	}
	ImGui::SameLine();

	// Cascade의 Time > Simulation Speed
	static constexpr float Speeds[] = { 0.01f, 0.1f, 0.25f, 0.5f, 1.0f };
	static constexpr const char* SpeedLabels[] = { "1%", "10%", "25%", "50%", "100%" };
	int32 Current = 4;
	for (int32 Index = 0; Index < IM_ARRAYSIZE(Speeds); ++Index)
	{
		if (Editor.GetSimulationSpeed() == Speeds[Index])
		{
			Current = Index;
		}
	}
	ImGui::SetNextItemWidth(80.0f);
	if (ImGui::BeginCombo("##SimSpeed", SpeedLabels[Current]))
	{
		for (int32 Index = 0; Index < IM_ARRAYSIZE(Speeds); ++Index)
		{
			if (ImGui::Selectable(SpeedLabels[Index], Index == Current))
			{
				Editor.SetSimulationSpeed(Speeds[Index]);
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();

	bool bAutoRestart = Editor.IsAutoRestart();
	if (ImGui::Checkbox("Loop", &bAutoRestart))
	{
		Editor.SetAutoRestart(bAutoRestart);
	}
	ImGui::SameLine();
	ImGui::TextDisabled("Particles: %d", Editor.GetActiveParticleCount());
}

void FParticleViewportPanel::DrawContent()
{
	DrawToolbar();

	FEditorViewportClient* Client = Editor.GetViewportClient();
	const ImVec2 Size = ImGui::GetContentRegionAvail();
	if (!Client || Size.x < 1.0f || Size.y < 1.0f)
	{
		return;
	}

	const ImVec2 Origin = ImGui::GetCursorScreenPos();

	// 이미지 영역을 입력 항목으로 등록하여 드래그 중에도 카메라 조작을 유지합니다.
	ImGui::InvisibleButton("##ParticleViewport", Size,
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	if (FTexture2D* Target = Client->GetColorTarget(); Target && Target->GetSRV())
	{
		ImGui::GetWindowDrawList()->AddImage(Target->GetSRV(), Origin,
			ImVec2(Origin.x + Size.x, Origin.y + Size.y));
	}

	const bool bHovered = ImGui::IsItemHovered();
	if (bHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
		ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
	{
		ImGui::SetWindowFocus();
	}

	const bool bFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	if (bFocused && bHovered && ImGui::IsKeyPressed(ImGuiKey_F, false))
	{
		Editor.FocusParticles();
	}
	if (bFocused && ImGui::IsKeyPressed(ImGuiKey_Space, false))
	{
		Editor.RestartSimulation();
	}

	const ImGuiIO& IO = ImGui::GetIO();
	FViewportCameraInput Input
	{
		.bCaptured		= bFocused && ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Right),
		.bAllowWheel	= bHovered,
		.bForward		= ImGui::IsKeyDown(ImGuiKey_W),
		.bBackward		= ImGui::IsKeyDown(ImGuiKey_S),
		.bRight			= ImGui::IsKeyDown(ImGuiKey_D),
		.bLeft			= ImGui::IsKeyDown(ImGuiKey_A),
		.bUp			= ImGui::IsKeyDown(ImGuiKey_E),
		.bDown			= ImGui::IsKeyDown(ImGuiKey_Q),
		.MouseX			= IO.MouseDelta.x,
		.MouseY			= IO.MouseDelta.y,
		// ImGui의 휠 단위를 기존 카메라 이동 함수가 사용하는 Windows 휠 단위로 변환합니다.
		.Wheel			= IO.MouseWheel * 120.0f,
	};

	Editor.SetViewportState(static_cast<uint32>(Size.x), static_cast<uint32>(Size.y), Input);
}

////////////////////////////////////////////////////////////
// Emitters
////////////////////////////////////////////////////////////

FParticleEmittersPanel::FParticleEmittersPanel(FParticleSystemEditor& InEditor)
	: FParticleSystemEditorPanel(InEditor, "Emitters", PanelIdPrefix)
{
}

void FParticleEmittersPanel::DrawModuleRow(UParticleEmitter* Emitter, UParticleLODLevel* LOD, UParticleModule* Module, const char* Kind, int32 Index)
{
	ImGui::PushID(Module);
	const float RowMinX = ImGui::GetCursorScreenPos().x;
	const bool bSelected = Editor.GetSelectedObject() == Module;
	const ImVec4 RowColor = GetModuleRowColor(Kind);
	// 색이 있는 행은 항상 자기 색으로 칠하고, 선택되면 기본 선택 색으로 보인다.
	const bool bColored = RowColor.w > 0.0f && !bSelected;
	if (bColored)
	{
		ImGui::PushStyleColor(ImGuiCol_Header, RowColor);
	}

	// Required/Spawn/TypeData는 이미터가 돌아가는 데 꼭 필요한 설정이라 끌 수 없다 (Cascade와 같다).
	const bool bFixed = Index < 0;
	if (!bFixed)
	{
		if (ImGui::Checkbox("##Enabled", &Module->bEnabled))
		{
			Editor.NotifyPropertyEdited();
		}
		ImGui::SameLine();
	}

	const FString Label = (std::strcmp(Kind, "Required") == 0 || std::strcmp(Kind, "Spawn") == 0)
		? FString(Kind)
		: GetModuleDisplayName(Module);
	if (ImGui::Selectable(Label.c_str(), bSelected || bColored))
	{
		Editor.SetSelectedObject(Module);
	}

	if (bColored)
	{
		ImGui::PopStyleColor();
	}

	// 우클릭: 모듈 삭제. Required/Spawn은 지울 수 없고, TypeData를 지우면 Sprite 이미터로 돌아간다.
	if (ImGui::BeginPopupContextItem("##ModuleContext"))
	{
		const bool bDeletable = !(Module == LOD->RequiredModule || Module == LOD->SpawnModule);
		if (ImGui::MenuItem("Delete Module", "Del", false, bDeletable))
		{
			PendingAction = [this, Emitter, Module] { Editor.DeleteModule(Emitter, Module); };
		}
		if (!bDeletable)
		{
			ImGui::SetItemTooltip("Required and Spawn modules cannot be removed.");
		}
		ImGui::EndPopup();
	}

	// 일반 모듈만 끌어서 순서를 바꿀 수 있다. Required/Spawn/TypeData는 항상 위에 고정이다.
	if (!bFixed)
	{
		if (ImGui::BeginDragDropSource())
		{
			const FModuleDragPayload Payload{ Emitter, Module };
			ImGui::SetDragDropPayload(ModulePayloadType, &Payload, sizeof(Payload));
			ImGui::TextUnformatted(Label.c_str());
			ImGui::EndDragDropSource();
		}

		// 이 행에 놓으면 이 자리(Index)로 간다. 같은 이미터에서 아래로 내리면 이 행 뒤에 들어가므로 선을 아래에 그린다.
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ModulePayloadType,
				ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
			{
				const FModuleDragPayload Data = *static_cast<const FModuleDragPayload*>(Payload->Data);
				const int32 SourceIndex = Data.Emitter == Emitter ? LOD->FindModuleIndex(Data.Module) : -1;
				const bool bBelow = SourceIndex >= 0 && SourceIndex < Index;
				DrawDropLine(RowMinX, ImGui::GetItemRectMax().x, bBelow ? ImGui::GetItemRectMax().y : ImGui::GetItemRectMin().y);

				if (Payload->IsDelivery() && Data.Module != Module)
				{
					PendingAction = [this, Data, Emitter, Index] { Editor.MoveModule(Data.Emitter, Data.Module, Emitter, Index); };
				}
			}
			ImGui::EndDragDropTarget();
		}
	}

	// Del 키로 선택한 모듈 삭제
	if (bSelected && !(Module == LOD->RequiredModule || Module == LOD->SpawnModule)
		&& ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !ImGui::IsAnyItemActive())
	{
		PendingAction = [this, Emitter, Module] { Editor.DeleteModule(Emitter, Module); };
	}

	ImGui::PopID();
}

void FParticleEmittersPanel::DrawAddModuleMenu(UParticleEmitter* Emitter)
{
	UParticleLODLevel* LOD = Emitter->GetLODLevel(0);

	// 리플렉션에 등록된 모듈 클래스를 모두 모은다. 새 모듈 클래스를 만들면 자동으로 메뉴에 나온다.
	TArray<UClass*> Classes;
	GetDerivedClasses(UParticleModule::StaticClass(), Classes, true);

	std::map<FString, std::vector<std::pair<FString, UClass*>>> Categories;
	std::vector<std::pair<FString, UClass*>> TypeDataClasses;
	for (UClass* Class : Classes)
	{
		if (!Class || !Class->Constructor
			|| Class == UParticleModule::StaticClass()
			|| Class == UParticleModuleTypeDataBase::StaticClass()
			|| Class->IsChildOf(UParticleModuleRequired::StaticClass())
			|| Class->IsChildOf(UParticleModuleSpawn::StaticClass()))
		{
			continue;
		}

		const FString Name = GetModuleClassDisplayName(Class);
		if (Class->IsChildOf(UParticleModuleTypeDataBase::StaticClass()))
		{
			TypeDataClasses.emplace_back(Name, Class);
			continue;
		}

		const FModuleClassInfo* Info = FindModuleClassInfo(Class);
		Categories[Info ? Info->Category : "Other"].emplace_back(Name, Class);
	}

	// TypeData는 이미터 종류를 바꾼다 (없으면 Sprite). Cascade처럼 맨 위에 둔다.
	if (!TypeDataClasses.empty() && ImGui::BeginMenu("TypeData"))
	{
		for (const auto& [Name, Class] : TypeDataClasses)
		{
			const bool bCurrent = LOD && LOD->TypeDataModule && LOD->TypeDataModule->GetClass() == Class;
			if (ImGui::MenuItem(("New " + Name).c_str(), nullptr, bCurrent, !bCurrent))
			{
				PendingAction = [this, Emitter, Class] { Editor.AddModule(Emitter, Class); };
			}
		}
		ImGui::EndMenu();
	}
	ImGui::Separator();

	for (auto& [Category, Entries] : Categories)
	{
		if (!ImGui::BeginMenu(Category.c_str()))
		{
			continue;
		}
		std::sort(Entries.begin(), Entries.end());
		for (const auto& [Name, Class] : Entries)
		{
			if (ImGui::MenuItem(Name.c_str()))
			{
				PendingAction = [this, Emitter, Class] { Editor.AddModule(Emitter, Class); };
			}
		}
		ImGui::EndMenu();
	}
}

void FParticleEmittersPanel::DrawEmitterColumn(UParticleEmitter* Emitter, float Width, float Height)
{
	ImGui::PushID(Emitter);
	ImGui::BeginChild("##EmitterColumn", ImVec2(Width, Height), ImGuiChildFlags_Borders);

	// 이미터 헤더: 켜기/끄기 + 이름
	bool bEmitterEnabled = !Emitter->IsDisabled();
	if (ImGui::Checkbox("##EmitterEnabled", &bEmitterEnabled))
	{
		Emitter->SetDisabled(!bEmitterEnabled);
		Editor.NotifyPropertyEdited();
	}
	ImGui::SameLine();
	const bool bEmitterSelected = Editor.GetSelectedObject() == Emitter;
	if (ImGui::Selectable(Emitter->GetEmitterName().c_str(), bEmitterSelected))
	{
		Editor.SetSelectedObject(Emitter);
	}

	// 헤더 우클릭: 이미터 편집
	if (ImGui::BeginPopupContextItem("##EmitterHeaderContext"))
	{
		if (ImGui::MenuItem("Rename Emitter"))
		{
			RenameTarget = Emitter;
			strncpy_s(RenameBuffer, Emitter->GetEmitterName().c_str(), _TRUNCATE);
			bOpenRenamePopup = true;   // 메뉴 팝업 안에서 바로 모달을 열 수 없어 패널 단계에서 연다
		}
		if (ImGui::MenuItem("Duplicate Emitter"))
		{
			PendingAction = [this, Emitter] { Editor.DuplicateEmitter(Emitter); };
		}
		if (ImGui::MenuItem("Delete Emitter", "Del"))
		{
			PendingAction = [this, Emitter] { Editor.DeleteEmitter(Emitter); };
		}
		ImGui::EndPopup();
	}

	if (bEmitterSelected && ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !ImGui::IsAnyItemActive())
	{
		PendingAction = [this, Emitter] { Editor.DeleteEmitter(Emitter); };
	}
	ImGui::Separator();

	// LOD/커브는 다음 단계에서 다룬다. 지금은 LOD 0만 표시한다.
	UParticleLODLevel* LOD = Emitter->GetLODLevel(0);
	if (LOD)
	{
		if (LOD->TypeDataModule)
		{
			DrawModuleRow(Emitter, LOD, LOD->TypeDataModule, "TypeData", -1);
		}
		if (LOD->RequiredModule)
		{
			DrawModuleRow(Emitter, LOD, LOD->RequiredModule, "Required", -1);
		}
		if (LOD->SpawnModule)
		{
			DrawModuleRow(Emitter, LOD, LOD->SpawnModule, "Spawn", -1);
		}

		const TArray<UParticleModule*>& Modules = LOD->GetModules();
		for (int32 Index = 0; Index < Modules.Num(); ++Index)
		{
			if (Modules[Index])
			{
				DrawModuleRow(Emitter, LOD, Modules[Index], "", Index);
			}
		}

		// 마지막 모듈 아래 빈 곳에 놓으면 맨 끝으로 간다. 항목이 아니라 영역이라 빈 곳 우클릭 메뉴를 막지 않는다.
		const ImVec2 TailMin = ImGui::GetCursorScreenPos();
		const ImVec2 WindowMax(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x, ImGui::GetWindowPos().y + ImGui::GetWindowSize().y);
		if (WindowMax.y > TailMin.y && ImGui::BeginDragDropTargetCustom(ImRect(TailMin, WindowMax), ImGui::GetID("##ModuleDropTail")))
		{
			if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ModulePayloadType,
				ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
			{
				DrawDropLine(TailMin.x, WindowMax.x - ImGui::GetStyle().WindowPadding.x, TailMin.y);
				if (Payload->IsDelivery())
				{
					const FModuleDragPayload Data = *static_cast<const FModuleDragPayload*>(Payload->Data);
					const int32 EndIndex = Modules.Num();
					PendingAction = [this, Data, Emitter, EndIndex] { Editor.MoveModule(Data.Emitter, Data.Module, Emitter, EndIndex); };
				}
			}
			ImGui::EndDragDropTarget();
		}
	}
	else
	{
		ImGui::TextDisabled("No LOD");
	}

	// 열의 빈 곳 우클릭: 모듈 추가
	if (LOD && ImGui::BeginPopupContextWindow("##EmitterColumnContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		ImGui::TextDisabled("%s", Emitter->GetEmitterName().c_str());
		ImGui::Separator();
		DrawAddModuleMenu(Emitter);
		ImGui::EndPopup();
	}

	ImGui::EndChild();
	ImGui::PopID();
}

void FParticleEmittersPanel::DrawRenamePopup()
{
	if (bOpenRenamePopup)
	{
		ImGui::OpenPopup("Rename Emitter");
		bOpenRenamePopup = false;
	}

	if (!ImGui::BeginPopupModal("Rename Emitter", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	if (ImGui::IsWindowAppearing())
	{
		ImGui::SetKeyboardFocusHere();
	}
	const bool bEnter = ImGui::InputText("Name", RenameBuffer, sizeof(RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

	ImGui::BeginDisabled(RenameBuffer[0] == '\0');
	if (ImGui::Button("OK") || (bEnter && RenameBuffer[0] != '\0'))
	{
		Editor.RenameEmitter(RenameTarget, RenameBuffer);
		RenameTarget = nullptr;
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
	{
		RenameTarget = nullptr;
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

void FParticleEmittersPanel::DrawContent()
{
	UParticleSystem* System = Editor.GetParticleSystem();
	if (!System)
	{
		return;
	}

	constexpr float ColumnWidth = 210.0f;
	const float ColumnHeight = ImGui::GetContentRegionAvail().y;

	const TArray<UParticleEmitter*>& Emitters = System->GetEmitters();
	bool bFirstColumn = true;
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (!Emitter)
		{
			continue;
		}
		if (!bFirstColumn)
		{
			ImGui::SameLine();
		}
		bFirstColumn = false;
		DrawEmitterColumn(Emitter, ColumnWidth, ColumnHeight);
	}

	if (bFirstColumn)
	{
		ImGui::TextDisabled("No emitters. Right-click to add one.");
	}
	else
	{
		// 이미터가 많아 가로로 넘쳐도 오른쪽에 우클릭할 빈 곳이 남도록 여백을 둔다.
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(ColumnWidth * 0.5f, 1.0f));
	}

	// 오른쪽 빈 곳 (이미터 열 밖) 우클릭: 새 이미터
	if (ImGui::BeginPopupContextWindow("##EmittersContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem("New Particle Sprite Emitter"))
		{
			PendingAction = [this] { Editor.AddSpriteEmitter(); };
		}		ImGui::EndPopup();
	}

	// 빈 곳을 누르면 시스템 자체를 선택한다 (Cascade와 같다).
	if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		Editor.SetSelectedObject(System);
	}

	DrawRenamePopup();

	if (PendingAction)
	{
		// 실행 중에 다시 PendingAction을 채울 수 있으므로 꺼내 놓고 부른다.
		const std::function<void()> Action = std::move(PendingAction);
		PendingAction = nullptr;
		Action();
	}
}

////////////////////////////////////////////////////////////
// Details
////////////////////////////////////////////////////////////

FParticleDetailsPanel::FParticleDetailsPanel(FParticleSystemEditor& InEditor)
	: FParticleSystemEditorPanel(InEditor, "Details", PanelIdPrefix)
{
}

void FParticleDetailsPanel::DrawContent()
{
	UObject* Selected = Editor.GetSelectedObject();
	if (!Selected)
	{
		ImGui::TextDisabled("Nothing selected");
		return;
	}

	if (Selected == Editor.GetParticleSystem())
	{
		ImGui::TextUnformatted("Particle System");
	}
	else if (const UParticleEmitter* Emitter = Cast<UParticleEmitter>(Selected))
	{
		ImGui::Text("Emitter: %s", Emitter->GetEmitterName().c_str());
	}
	else
	{
		ImGui::Text("Module: %s", GetModuleDisplayName(Selected).c_str());
	}
	ImGui::Separator();

	// Required의 Material을 메시 이미터가 언제 쓰는지 알려 준다.
	if (const UParticleModuleRequired* Required = Cast<UParticleModuleRequired>(Selected))
	{
		DrawRequiredMaterialHint(Required);
	}

	// Ribbon TypeData has no settings of its own yet; explain where the strip's look comes from.
	if (Cast<UParticleModuleTypeDataRibbon>(Selected))
	{
		ImGui::TextWrapped("Ribbon: particles are linked in spawn order and drawn as one camera-facing strip.");
		ImGui::BulletText("Width = particle Size X (Initial Size / Size By Life)");
		ImGui::BulletText("Color / alpha = particle color (Initial Color / Color Over Life)");
		ImGui::BulletText("Turn off Required > bUseLocalSpace to leave a trail when the emitter moves");
		ImGui::BulletText("Needs at least 2 live particles to draw");
		ImGui::Separator();
	}

	// 그룹으로 묶어 안쪽 항목 중 하나라도 바뀌었는지 확인한다. 바뀌면 편집이 끝난 뒤 시뮬레이션을 다시 시작한다.
	// 콤보 팝업 안의 선택은 그룹의 Edited로 잡히지 않을 수 있어, 머티리얼처럼 포인터가 바뀌는 값은 따로 비교한다.
	ImGui::PushID(Selected);
	const UParticleModuleRequired* Required = Cast<UParticleModuleRequired>(Selected);
	const auto MaterialState = [Required]()
	{
		const UMaterial* Material = Required ? Required->Material : nullptr;
		return std::make_tuple(Material,
			Material ? Material->PSOType : EPSOType::Count,
			Material && Material->Textures.IsValidIndex(0) ? Material->Textures[0] : nullptr);
	};
	const auto MaterialBefore = MaterialState();
	ImGui::BeginGroup();
	FPropertyEditor::DrawObjectProperties(Selected);
	ImGui::EndGroup();
	if (ImGui::IsItemEdited() || MaterialState() != MaterialBefore)
	{
		Editor.NotifyPropertyEdited();
	}

	if (UParticleModuleSpawn* Spawn = Cast<UParticleModuleSpawn>(Selected))
	{
		DrawBurstList(Spawn);
	}
	ImGui::PopID();
}

void FParticleDetailsPanel::DrawRequiredMaterialHint(const UParticleModuleRequired* Required)
{
	// 이 Required를 가진 LOD를 찾아 TypeData를 본다.
	const UParticleModuleTypeDataMesh* MeshType = nullptr;
	bool bRibbon = false;
	if (UParticleSystem* System = Editor.GetParticleSystem())
	{
		for (UParticleEmitter* Emitter : System->GetEmitters())
		{
			for (UParticleLODLevel* LOD : Emitter ? Emitter->GetLODLevels() : TArray<UParticleLODLevel*>())
			{
				if (LOD && LOD->RequiredModule == Required)
				{
					MeshType = Cast<UParticleModuleTypeDataMesh>(LOD->TypeDataModule);
					// Beam도 Ribbon 띠 경로로 그려지므로 같은 머티리얼 규칙을 따른다
					bRibbon = Cast<UParticleModuleTypeDataRibbon>(LOD->TypeDataModule) != nullptr
						|| Cast<UParticleModuleTypeDataBeam2>(LOD->TypeDataModule) != nullptr;
				}
			}
		}
	}

	if (bRibbon)
	{
		const bool bRibbonMaterial = Required->Material && Required->Material->PSOType == EPSOType::Particle_Ribbon;
		if (bRibbonMaterial)
		{
			ImGui::TextDisabled("Ribbon emitter: the strip is drawn with this Material.");
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "Ribbon emitter: this Material is not a Ribbon material.");
			ImGui::TextDisabled("RibbonMaterial is used instead. Pick a Ribbon material to change the look.");
		}
		ImGui::Separator();
		return;
	}

	if (!MeshType)
	{
		ImGui::TextDisabled("Sprite emitter: particles are drawn with this Material.");
	}
	else if (MeshType->bOverrideMaterial)
	{
		ImGui::TextDisabled("Mesh emitter: this Material replaces the mesh's own materials (bOverrideMaterial is on).");
		ImGui::TextDisabled("Use Blend = Translucent for Color Over Life alpha to fade the meshes.");
	}
	else
	{
		ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "Mesh emitter: this Material is ignored.");
		ImGui::TextDisabled("The mesh's own materials are used. Turn on Mesh Data > bOverrideMaterial to use it.");
	}
	ImGui::Separator();
}

void FParticleDetailsPanel::DrawBurstList(UParticleModuleSpawn* Spawn)
{
	ImGui::Separator();
	TArray<FParticleBurst>& Bursts = Spawn->GetMutableBurstList();
	ImGui::Text("Burst List (%d)", Bursts.Num());
	ImGui::SameLine();
	if (ImGui::SmallButton("+ Add"))
	{
		Bursts.Add(FParticleBurst{ .Count = 10, .CountLow = -1, .Time = 0.0f });
		Editor.NotifyPropertyEdited();
	}
	ImGui::TextDisabled("Time is a fraction (0-1) of EmitterDuration. CountLow -1 = always Count, otherwise random in [CountLow, Count].");

	if (Bursts.IsEmpty())
	{
		return;
	}

	int32 RemoveIndex = -1;
	bool bEdited = false;
	if (ImGui::BeginTable("##BurstTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("CountLow", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("##Remove", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
		ImGui::TableHeadersRow();

		for (int32 Index = 0; Index < Bursts.Num(); ++Index)
		{
			FParticleBurst& Burst = Bursts[Index];
			ImGui::PushID(Index);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::SetNextItemWidth(-1.0f);
			bEdited |= ImGui::DragFloat("##Time", &Burst.Time, 0.01f, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);

			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::DragInt("##Count", &Burst.Count, 0.2f, 0, 100000))
			{
				Burst.Count = std::max(Burst.Count, 0);
				bEdited = true;
			}

			ImGui::TableSetColumnIndex(2);
			ImGui::SetNextItemWidth(-1.0f);
			bEdited |= ImGui::DragInt("##CountLow", &Burst.CountLow, 0.2f, -1, 100000);

			ImGui::TableSetColumnIndex(3);
			if (ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), 0.0f)))
			{
				RemoveIndex = Index;
			}
			ImGui::SetItemTooltip("Remove this burst");
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	if (RemoveIndex >= 0)
	{
		Bursts.RemoveAt(static_cast<uint32>(RemoveIndex));
		bEdited = true;
	}
	if (bEdited)
	{
		Editor.NotifyPropertyEdited();
	}
}

////////////////////////////////////////////////////////////
// Curve Editor
////////////////////////////////////////////////////////////

namespace
{
	bool DragCurveValue(const char* Id, float& Value) { return ImGui::DragFloat(Id, &Value, 0.01f, 0.0f, 0.0f, "%.3f"); }
	bool DragCurveValue(const char* Id, FVector& Value) { return ImGui::DragFloat3(Id, Value.V, 0.01f, 0.0f, 0.0f, "%.3f"); }

	// Key table for one curve: Time | Value | Mode | Tangent | X. Returns true when the curve changed.
	// Time is clamped between the neighbouring keys instead of re-sorting, so rows (and their ImGui ids) never swap mid-drag.
	template <typename T>
	bool DrawCurveKeyTable(FInterpCurve<T>& Curve)
	{
		using FPoint = FInterpCurvePoint<T>;
		bool bEdited = false;
		int32 RemoveIndex = -1;

		if (ImGui::BeginTable("##Keys", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthStretch, 0.7f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, TInterpCurveTraits<T>::NumComponents > 1 ? 2.0f : 0.8f);
			ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableSetupColumn("Tangent", ImGuiTableColumnFlags_WidthStretch, TInterpCurveTraits<T>::NumComponents > 1 ? 2.0f : 0.8f);
			ImGui::TableSetupColumn("##Remove", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
			ImGui::TableHeadersRow();

			for (int32 Index = 0; Index < Curve.Num(); ++Index)
			{
				FPoint& Point = Curve.Points[Index];
				ImGui::PushID(Index);
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::SetNextItemWidth(-1.0f);
				const float MinTime = Index > 0 ? Curve.Points[Index - 1].InVal : -FLT_MAX;
				const float MaxTime = Index + 1 < Curve.Num() ? Curve.Points[Index + 1].InVal : FLT_MAX;
				if (ImGui::DragFloat("##Time", &Point.InVal, 0.005f, MinTime, MaxTime, "%.3f", ImGuiSliderFlags_AlwaysClamp))
				{
					bEdited = true;
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1.0f);
				bEdited |= DragCurveValue("##Value", Point.OutVal);

				ImGui::TableSetColumnIndex(2);
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##Mode", InterpCurveModeToName(Point.InterpMode)))
				{
					for (int32 ModeIndex = 0; ModeIndex <= static_cast<int32>(EInterpCurveMode::Constant); ++ModeIndex)
					{
						const EInterpCurveMode Mode = static_cast<EInterpCurveMode>(ModeIndex);
						if (ImGui::Selectable(InterpCurveModeToName(Mode), Point.InterpMode == Mode))
						{
							Point.InterpMode = Mode;
							bEdited = true;
						}
					}
					ImGui::EndCombo();
				}
				ImGui::SetItemTooltip("Shape of the segment from this key to the next one");

				// Auto modes compute tangents; User edits one tangent for both sides; Break edits each side.
				ImGui::TableSetColumnIndex(3);
				ImGui::SetNextItemWidth(-1.0f);
				if (Point.InterpMode == EInterpCurveMode::CurveUser)
				{
					if (DragCurveValue("##Tangent", Point.LeaveTangent))
					{
						Point.ArriveTangent = Point.LeaveTangent;
						bEdited = true;
					}
				}
				else if (Point.InterpMode == EInterpCurveMode::CurveBreak)
				{
					bEdited |= DragCurveValue("##Arrive", Point.ArriveTangent);
					ImGui::SetItemTooltip("Arrive tangent");
					ImGui::SetNextItemWidth(-1.0f);
					bEdited |= DragCurveValue("##Leave", Point.LeaveTangent);
					ImGui::SetItemTooltip("Leave tangent");
				}
				else
				{
					ImGui::TextDisabled(Point.IsCurveKey() ? "auto" : "-");
				}

				ImGui::TableSetColumnIndex(4);
				ImGui::BeginDisabled(Curve.Num() <= 1);
				if (ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), 0.0f)))
				{
					RemoveIndex = Index;
				}
				ImGui::EndDisabled();
				ImGui::SetItemTooltip("Remove this key");
				ImGui::PopID();
			}
			ImGui::EndTable();
		}

		if (RemoveIndex >= 0)
		{
			Curve.RemovePoint(RemoveIndex);
			return true;
		}
		if (bEdited)
		{
			// Two keys at the same time make the neighbour clamp a no-op in ImGui, so keep Eval's sorted order anyway.
			std::stable_sort(Curve.Points.begin(), Curve.Points.end(),
				[](const FPoint& A, const FPoint& B) { return A.InVal < B.InVal; });
			Curve.AutoSetTangents();
		}
		return bEdited;
	}

	// Adds a key in the middle of the widest gap, using the curve's current value there so the shape does not change.
	template <typename T>
	void AddCurveKeyInWidestGap(FInterpCurve<T>& Curve, const T& Default)
	{
		if (Curve.Num() == 0)
		{
			Curve.AddPoint(0.0f, Default);
			return;
		}
		if (Curve.Num() == 1)
		{
			const FInterpCurvePoint<T>& Only = Curve.Points[0];
			Curve.AddPoint(Only.InVal + 1.0f, Only.OutVal);
			return;
		}

		int32 GapIndex = 0;
		for (int32 Index = 1; Index + 1 < Curve.Num(); ++Index)
		{
			const float Gap = Curve.Points[Index + 1].InVal - Curve.Points[Index].InVal;
			if (Gap > Curve.Points[GapIndex + 1].InVal - Curve.Points[GapIndex].InVal)
			{
				GapIndex = Index;
			}
		}
		const float Time = 0.5f * (Curve.Points[GapIndex].InVal + Curve.Points[GapIndex + 1].InVal);
		const EInterpCurveMode Mode = Curve.Points[GapIndex].InterpMode;
		Curve.AddPoint(Time, Curve.Eval(Time, Default), Mode);
	}

	// One collapsible section per curve property. Returns true when the curve changed.
	template <typename T>
	bool DrawCurveSection(const FString& Name, FInterpCurve<T>& Curve, const T& Default)
	{
		ImGui::PushID(Name.c_str());
		bool bEdited = false;
		if (ImGui::CollapsingHeader(Name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::SmallButton("+ Key"))
			{
				AddCurveKeyInWidestGap(Curve, Default);
				bEdited = true;
			}
			ImGui::SameLine();
			ImGui::TextDisabled("%d keys", Curve.Num());
			bEdited |= DrawCurveKeyTable(Curve);
		}
		ImGui::PopID();
		return bEdited;
	}
}

namespace
{
	// One curve property of the selected module. Float and vector curves behind one interface,
	// so the graph treats every curve as "N components, each a float over time".
	struct FCurveHandle
	{
		const FProperty* Property = nullptr;
		FInterpCurveFloat* FloatCurve = nullptr;
		FInterpCurveVector* VectorCurve = nullptr;

		template <typename FuncType>
		decltype(auto) Visit(FuncType&& Func) const { return FloatCurve ? Func(*FloatCurve) : Func(*VectorCurve); }

		const FString& Name() const { return Property->Name; }
		int32 NumComponents() const { return FloatCurve ? 1 : 3; }
		int32 NumKeys() const { return Visit([](auto& C) { return C.Num(); }); }

		float KeyTime(int32 Key) const { return Visit([Key](auto& C) { return C.Points[Key].InVal; }); }
		EInterpCurveMode KeyMode(int32 Key) const { return Visit([Key](auto& C) { return C.Points[Key].InterpMode; }); }

		float KeyValue(int32 Key, int32 Comp) const
		{
			return Visit([&](auto& C)
			{
				using T = typename std::decay_t<decltype(C)>::ValueType;
				return TInterpCurveTraits<T>::Component(C.Points[Key].OutVal, Comp);
			});
		}

		float Tangent(int32 Key, int32 Comp, bool bArrive) const
		{
			return Visit([&](auto& C)
			{
				using T = typename std::decay_t<decltype(C)>::ValueType;
				const auto& Point = C.Points[Key];
				return TInterpCurveTraits<T>::Component(bArrive ? Point.ArriveTangent : Point.LeaveTangent, Comp);
			});
		}

		float Eval(float Time, int32 Comp) const
		{
			return Visit([&](auto& C)
			{
				using T = typename std::decay_t<decltype(C)>::ValueType;
				return TInterpCurveTraits<T>::Component(C.Eval(Time), Comp);
			});
		}

		void SetKeyTime(int32 Key, float Time) const { Visit([&](auto& C) { C.Points[Key].InVal = Time; }); }
		void SetKeyMode(int32 Key, EInterpCurveMode Mode) const { Visit([&](auto& C) { C.Points[Key].InterpMode = Mode; }); }

		void SetKeyValue(int32 Key, int32 Comp, float Value) const
		{
			Visit([&](auto& C)
			{
				using T = typename std::decay_t<decltype(C)>::ValueType;
				TInterpCurveTraits<T>::Component(C.Points[Key].OutVal, Comp) = Value;
			});
		}

		void SetTangent(int32 Key, int32 Comp, bool bArrive, float Value) const
		{
			Visit([&](auto& C)
			{
				using T = typename std::decay_t<decltype(C)>::ValueType;
				auto& Point = C.Points[Key];
				TInterpCurveTraits<T>::Component(bArrive ? Point.ArriveTangent : Point.LeaveTangent, Comp) = Value;
			});
		}

		// New key at Time on the current curve (shape unchanged), with the mode of the key to its left.
		int32 AddKey(float Time) const
		{
			return Visit([&](auto& C)
			{
				EInterpCurveMode Mode = EInterpCurveMode::Linear;
				for (const auto& Point : C.Points)
				{
					if (Point.InVal <= Time) Mode = Point.InterpMode;
				}
				return C.AddPoint(Time, C.Eval(Time), Mode);
			});
		}

		void RemoveKey(int32 Key) const { Visit([Key](auto& C) { C.RemovePoint(Key); }); }
		void AutoSetTangents() const { Visit([](auto& C) { C.AutoSetTangents(); }); }
	};

	TArray<FCurveHandle> GatherCurves(UObject* Object)
	{
		TArray<FCurveHandle> Curves;
		for (const UStruct* Struct = Object->GetClass(); Struct; Struct = Struct->GetSuperStruct())
		{
			for (const FProperty* Property : Struct->GetProperties())
			{
				FCurveHandle Handle;
				Handle.Property = Property;
				if (const FInterpCurveFloatProperty* FloatProperty = CastField<FInterpCurveFloatProperty>(Property))
				{
					Handle.FloatCurve = &FloatProperty->GetPropertyValue_InContainer(Object);
					Curves.Add(Handle);
				}
				else if (const FInterpCurveVectorProperty* VectorProperty = CastField<FInterpCurveVectorProperty>(Property))
				{
					Handle.VectorCurve = &VectorProperty->GetPropertyValue_InContainer(Object);
					Curves.Add(Handle);
				}
				// Distributions show up only while their type is Curve (Constant / Uniform are edited in Details)
				else if (const FDistributionFloatProperty* FloatDistribution = CastField<FDistributionFloatProperty>(Property))
				{
					FDistributionFloat& Distribution = FloatDistribution->GetPropertyValue_InContainer(Object);
					if (Distribution.Type == EDistributionType::Curve)
					{
						Handle.FloatCurve = &Distribution.Curve;
						Curves.Add(Handle);
					}
				}
				else if (const FDistributionVectorProperty* VectorDistribution = CastField<FDistributionVectorProperty>(Property))
				{
					FDistributionVector& Distribution = VectorDistribution->GetPropertyValue_InContainer(Object);
					if (Distribution.Type == EDistributionType::Curve)
					{
						Handle.VectorCurve = &Distribution.Curve;
						Curves.Add(Handle);
					}
				}
			}
		}
		return Curves;
	}

	// Line color per component: float curves white, vector X/Y/Z red/green/blue (also R/G/B for color curves)
	ImU32 CurveComponentColor(const FCurveHandle& Curve, int32 Comp, float Alpha = 1.0f)
	{
		const int32 A = static_cast<int32>(255.0f * Alpha);
		if (Curve.NumComponents() == 1) return IM_COL32(225, 225, 225, A);
		switch (Comp)
		{
		case 0:  return IM_COL32(235, 85, 85, A);
		case 1:  return IM_COL32(95, 205, 95, A);
		default: return IM_COL32(95, 145, 245, A);
		}
	}

	const char* ComponentLabel(int32 Comp)
	{
		static const char* Labels[] = { "X", "Y", "Z" };
		return Labels[std::clamp(Comp, 0, 2)];
	}

	bool IsComponentVisible(const FCurveGraphState& State, const FCurveHandle& Curve, int32 Comp)
	{
		const auto It = State.HiddenMask.find(Curve.Name());
		return It == State.HiddenMask.end() || (It->second & (1u << Comp)) == 0;
	}

	// Curve units <-> canvas pixels
	struct FGraphTransform
	{
		ImVec2 Min;
		ImVec2 Max;
		const FCurveGraphState* State = nullptr;

		float ScaleX() const { return (Max.x - Min.x) / (State->ViewMaxX - State->ViewMinX); } // pixels per unit
		float ScaleY() const { return (Max.y - Min.y) / (State->ViewMaxY - State->ViewMinY); }
		float ToScreenX(float X) const { return Min.x + (X - State->ViewMinX) * ScaleX(); }
		float ToScreenY(float Y) const { return Max.y - (Y - State->ViewMinY) * ScaleY(); }
		ImVec2 ToScreen(float X, float Y) const { return ImVec2(ToScreenX(X), ToScreenY(Y)); }
		float ToCurveX(float ScreenX) const { return State->ViewMinX + (ScreenX - Min.x) / ScaleX(); }
		float ToCurveY(float ScreenY) const { return State->ViewMinY + (Max.y - ScreenY) / ScaleY(); }
	};

	// Grid spacing of 1/2/5 x 10^n that keeps lines at least MinPixels apart
	float NiceGridStep(float Range, float Pixels, float MinPixels)
	{
		const float Raw = Range * MinPixels / std::max(Pixels, 1.0f);
		const float Power = std::pow(10.0f, std::floor(std::log10(std::max(Raw, 1e-6f))));
		for (const float Multiplier : { 1.0f, 2.0f, 5.0f, 10.0f })
		{
			if (Power * Multiplier >= Raw) return Power * Multiplier;
		}
		return Power * 10.0f;
	}

	// Frames every visible component, including overshoot between keys. X always covers 0..1 (both RelativeTime and the emitter loop fraction run 0..1).
	void FitView(FCurveGraphState& State, const TArray<FCurveHandle>& Curves)
	{
		float MinX = 0.0f, MaxX = 1.0f;
		float MinY = FLT_MAX, MaxY = -FLT_MAX;
		for (const FCurveHandle& Curve : Curves)
		{
			const int32 NumKeys = Curve.NumKeys();
			if (NumKeys == 0) continue;
			MinX = std::min(MinX, Curve.KeyTime(0));
			MaxX = std::max(MaxX, Curve.KeyTime(NumKeys - 1));
		}
		for (const FCurveHandle& Curve : Curves)
		{
			for (int32 Comp = 0; Comp < Curve.NumComponents(); ++Comp)
			{
				if (!IsComponentVisible(State, Curve, Comp) || Curve.NumKeys() == 0) continue;
				constexpr int32 Samples = 64;
				for (int32 i = 0; i <= Samples; ++i)
				{
					const float Value = Curve.Eval(MinX + (MaxX - MinX) * i / Samples, Comp);
					MinY = std::min(MinY, Value);
					MaxY = std::max(MaxY, Value);
				}
			}
		}
		if (MinY > MaxY) { MinY = 0.0f; MaxY = 1.0f; }
		if (MaxY - MinY < 1e-3f) { MinY -= 0.5f; MaxY += 0.5f; }

		const float PadX = (MaxX - MinX) * 0.05f;
		const float PadY = (MaxY - MinY) * 0.12f;
		State.ViewMinX = MinX - PadX;
		State.ViewMaxX = MaxX + PadX;
		State.ViewMinY = MinY - PadY;
		State.ViewMaxY = MaxY + PadY;
		State.bFitRequested = false;
	}

	// Screen offset of a tangent handle from its key: fixed pixel length along the tangent's on-screen slope
	ImVec2 TangentHandleOffset(const FGraphTransform& Transform, float Tangent, bool bArrive)
	{
		constexpr float HandleLength = 45.0f;
		ImVec2 Dir(Transform.ScaleX(), -Tangent * Transform.ScaleY());
		const float Length = std::sqrt(Dir.x * Dir.x + Dir.y * Dir.y);
		if (Length < 1e-6f) Dir = ImVec2(1.0f, 0.0f);
		else Dir = ImVec2(Dir.x / Length, Dir.y / Length);
		const float Sign = bArrive ? -1.0f : 1.0f;
		return ImVec2(Dir.x * HandleLength * Sign, Dir.y * HandleLength * Sign);
	}

	int32 FindCurve(const TArray<FCurveHandle>& Curves, const FString& Name)
	{
		for (int32 i = 0; i < Curves.Num(); ++i)
		{
			if (Curves[i].Name() == Name) return i;
		}
		return -1;
	}

	// Toolbar + canvas. Returns the index of the curve changed this frame, or -1.
	int32 DrawCurveGraph(FCurveGraphState& State, const TArray<FCurveHandle>& Curves)
	{
		ImGuiIO& IO = ImGui::GetIO();

		// Drop a selection that no longer exists (key deleted from the table, other module, ...)
		int32 SelectedCurveIndex = FindCurve(Curves, State.SelectedCurve);
		if (SelectedCurveIndex < 0 || State.SelectedKey < 0 || State.SelectedKey >= Curves[SelectedCurveIndex].NumKeys())
		{
			SelectedCurveIndex = -1;
			State.SelectedKey = -1;
		}

		// ---- Toolbar: Fit + visibility per curve / component ----
		if (ImGui::SmallButton("Fit"))
		{
			State.bFitRequested = true;
		}
		ImGui::SetItemTooltip("Frame all visible curves (F)");
		for (const FCurveHandle& Curve : Curves)
		{
			ImGui::SameLine(0.0f, 14.0f);
			ImGui::PushID(Curve.Name().c_str());
			uint8& Mask = State.HiddenMask[Curve.Name()];
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(Curve.Name().c_str());
			for (int32 Comp = 0; Comp < Curve.NumComponents(); ++Comp)
			{
				ImGui::SameLine(0.0f, 4.0f);
				bool bVisible = (Mask & (1u << Comp)) == 0;
				ImGui::PushID(Comp);
				ImGui::PushStyleColor(ImGuiCol_CheckMark, CurveComponentColor(Curve, Comp));
				if (ImGui::Checkbox(Curve.NumComponents() > 1 ? ComponentLabel(Comp) : "##Show", &bVisible))
				{
					Mask = static_cast<uint8>(bVisible ? (Mask & ~(1u << Comp)) : (Mask | (1u << Comp)));
				}
				ImGui::PopStyleColor();
				ImGui::PopID();
			}
			ImGui::PopID();
		}

		// ---- Canvas ----
		const ImVec2 Avail = ImGui::GetContentRegionAvail();
		const ImVec2 CanvasSize(std::max(Avail.x, 100.0f), std::max(Avail.y * 0.65f, 180.0f));
		const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
		const ImVec2 CanvasMax(CanvasMin.x + CanvasSize.x, CanvasMin.y + CanvasSize.y);
		ImGui::InvisibleButton("##CurveCanvas", CanvasSize,
			ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
		const bool bHovered = ImGui::IsItemHovered();
		const bool bActive = ImGui::IsItemActive();
		ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY); // wheel zooms the graph instead of scrolling the panel

		if (State.bFitRequested || (bHovered && ImGui::IsKeyPressed(ImGuiKey_F, false)))
		{
			FitView(State, Curves);
		}

		const FGraphTransform Transform{ CanvasMin, CanvasMax, &State };
		const ImVec2 Mouse = IO.MousePos;
		const float MouseX = Transform.ToCurveX(Mouse.x);
		const float MouseY = Transform.ToCurveY(Mouse.y);

		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->PushClipRect(CanvasMin, CanvasMax, true);
		DrawList->AddRectFilled(CanvasMin, CanvasMax, IM_COL32(30, 30, 33, 255));

		// Both curve inputs run 0..1: shade the rest so it is clear those parts are never reached
		const float Domain0 = Transform.ToScreenX(0.0f);
		const float Domain1 = Transform.ToScreenX(1.0f);
		if (Domain0 > CanvasMin.x) DrawList->AddRectFilled(CanvasMin, ImVec2(Domain0, CanvasMax.y), IM_COL32(0, 0, 0, 60));
		if (Domain1 < CanvasMax.x) DrawList->AddRectFilled(ImVec2(Domain1, CanvasMin.y), CanvasMax, IM_COL32(0, 0, 0, 60));

		// Grid + labels
		char LabelBuffer[32];
		const float StepX = NiceGridStep(State.ViewMaxX - State.ViewMinX, CanvasSize.x, 70.0f);
		for (float X = std::ceil(State.ViewMinX / StepX) * StepX; X <= State.ViewMaxX; X += StepX)
		{
			const float SX = Transform.ToScreenX(X);
			const bool bAxis = std::fabs(X) < StepX * 0.01f;
			DrawList->AddLine(ImVec2(SX, CanvasMin.y), ImVec2(SX, CanvasMax.y), bAxis ? IM_COL32(110, 110, 115, 255) : IM_COL32(55, 55, 60, 255));
			std::snprintf(LabelBuffer, sizeof(LabelBuffer), "%.3g", std::fabs(X) < StepX * 0.01f ? 0.0f : X);
			DrawList->AddText(ImVec2(SX + 3.0f, CanvasMax.y - ImGui::GetTextLineHeight() - 2.0f), IM_COL32(140, 140, 145, 255), LabelBuffer);
		}
		const float StepY = NiceGridStep(State.ViewMaxY - State.ViewMinY, CanvasSize.y, 40.0f);
		for (float Y = std::ceil(State.ViewMinY / StepY) * StepY; Y <= State.ViewMaxY; Y += StepY)
		{
			const float SY = Transform.ToScreenY(Y);
			const bool bAxis = std::fabs(Y) < StepY * 0.01f;
			DrawList->AddLine(ImVec2(CanvasMin.x, SY), ImVec2(CanvasMax.x, SY), bAxis ? IM_COL32(110, 110, 115, 255) : IM_COL32(55, 55, 60, 255));
			std::snprintf(LabelBuffer, sizeof(LabelBuffer), "%.3g", bAxis ? 0.0f : Y);
			DrawList->AddText(ImVec2(CanvasMin.x + 3.0f, SY - ImGui::GetTextLineHeight()), IM_COL32(140, 140, 145, 255), LabelBuffer);
		}

		// Curves, sampled every 2 pixels
		std::vector<ImVec2> LinePoints;
		for (int32 CurveIndex = 0; CurveIndex < Curves.Num(); ++CurveIndex)
		{
			const FCurveHandle& Curve = Curves[CurveIndex];
			if (Curve.NumKeys() == 0) continue;
			for (int32 Comp = 0; Comp < Curve.NumComponents(); ++Comp)
			{
				if (!IsComponentVisible(State, Curve, Comp)) continue;
				LinePoints.clear();
				for (float SX = CanvasMin.x; SX <= CanvasMax.x + 2.0f; SX += 2.0f)
				{
					LinePoints.push_back(ImVec2(SX, Transform.ToScreenY(Curve.Eval(Transform.ToCurveX(SX), Comp))));
				}
				const bool bSelectedCurve = CurveIndex == SelectedCurveIndex && Comp == State.SelectedComponent;
				DrawList->AddPolyline(LinePoints.data(), static_cast<int>(LinePoints.size()),
					CurveComponentColor(Curve, Comp, bSelectedCurve ? 1.0f : 0.8f), ImDrawFlags_None, bSelectedCurve ? 2.5f : 1.5f);
			}
		}

		// Key under the mouse (closest within 7 px)
		int32 HoverCurve = -1, HoverKey = -1, HoverComp = 0;
		float HoverDistSq = 7.0f * 7.0f;
		for (int32 CurveIndex = 0; bHovered && CurveIndex < Curves.Num(); ++CurveIndex)
		{
			const FCurveHandle& Curve = Curves[CurveIndex];
			for (int32 Key = 0; Key < Curve.NumKeys(); ++Key)
			{
				for (int32 Comp = 0; Comp < Curve.NumComponents(); ++Comp)
				{
					if (!IsComponentVisible(State, Curve, Comp)) continue;
					const ImVec2 P = Transform.ToScreen(Curve.KeyTime(Key), Curve.KeyValue(Key, Comp));
					const float DistSq = (P.x - Mouse.x) * (P.x - Mouse.x) + (P.y - Mouse.y) * (P.y - Mouse.y);
					if (DistSq < HoverDistSq)
					{
						HoverDistSq = DistSq;
						HoverCurve = CurveIndex;
						HoverKey = Key;
						HoverComp = Comp;
					}
				}
			}
		}

		// Tangent handles of the selected key (only for smooth keys, where tangents matter)
		bool bShowHandles = false;
		ImVec2 KeyScreen, ArriveScreen, LeaveScreen;
		if (SelectedCurveIndex >= 0)
		{
			const FCurveHandle& Curve = Curves[SelectedCurveIndex];
			const int32 Key = State.SelectedKey;
			const int32 Comp = State.SelectedComponent;
			bShowHandles = IsInterpCurveModeCurve(Curve.KeyMode(Key)) && IsComponentVisible(State, Curve, Comp);
			if (bShowHandles)
			{
				KeyScreen = Transform.ToScreen(Curve.KeyTime(Key), Curve.KeyValue(Key, Comp));
				const ImVec2 ArriveOffset = TangentHandleOffset(Transform, Curve.Tangent(Key, Comp, true), true);
				const ImVec2 LeaveOffset = TangentHandleOffset(Transform, Curve.Tangent(Key, Comp, false), false);
				ArriveScreen = ImVec2(KeyScreen.x + ArriveOffset.x, KeyScreen.y + ArriveOffset.y);
				LeaveScreen = ImVec2(KeyScreen.x + LeaveOffset.x, KeyScreen.y + LeaveOffset.y);
			}
		}
		const auto IsNear = [&Mouse](const ImVec2& P) { return (P.x - Mouse.x) * (P.x - Mouse.x) + (P.y - Mouse.y) * (P.y - Mouse.y) < 7.0f * 7.0f; };

		// ---- Input ----
		int32 EditedCurve = -1;

		if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			if (bShowHandles && IsNear(LeaveScreen))
			{
				State.Drag = FCurveGraphState::EDrag::LeaveTangent;
			}
			else if (bShowHandles && IsNear(ArriveScreen))
			{
				State.Drag = FCurveGraphState::EDrag::ArriveTangent;
			}
			else if (HoverCurve >= 0)
			{
				State.SelectedCurve = Curves[HoverCurve].Name();
				State.SelectedKey = HoverKey;
				State.SelectedComponent = HoverComp;
				SelectedCurveIndex = HoverCurve;
				State.Drag = FCurveGraphState::EDrag::Key;
			}
			else
			{
				State.SelectedKey = -1;
				SelectedCurveIndex = -1;
				State.Drag = FCurveGraphState::EDrag::None;
			}
		}

		// Double-click empty space: key on the visible component whose line is closest to the mouse, at the mouse value
		if (bHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && HoverCurve < 0)
		{
			int32 BestCurve = -1, BestComp = 0;
			float BestDist = FLT_MAX;
			for (int32 CurveIndex = 0; CurveIndex < Curves.Num(); ++CurveIndex)
			{
				for (int32 Comp = 0; Comp < Curves[CurveIndex].NumComponents(); ++Comp)
				{
					if (!IsComponentVisible(State, Curves[CurveIndex], Comp)) continue;
					const float Dist = std::fabs(Transform.ToScreenY(Curves[CurveIndex].Eval(MouseX, Comp)) - Mouse.y);
					if (Dist < BestDist)
					{
						BestDist = Dist;
						BestCurve = CurveIndex;
						BestComp = Comp;
					}
				}
			}
			if (BestCurve >= 0)
			{
				const FCurveHandle& Curve = Curves[BestCurve];
				const int32 NewKey = Curve.AddKey(MouseX);
				Curve.SetKeyValue(NewKey, BestComp, MouseY);
				Curve.AutoSetTangents();
				State.SelectedCurve = Curve.Name();
				State.SelectedKey = NewKey;
				State.SelectedComponent = BestComp;
				SelectedCurveIndex = BestCurve;
				State.Drag = FCurveGraphState::EDrag::Key;
				EditedCurve = BestCurve;
			}
		}

		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && State.Drag != FCurveGraphState::EDrag::Pan)
		{
			State.Drag = FCurveGraphState::EDrag::None;
		}

		// Dragging a key: time stays between its neighbours (so key indices never swap), value follows the mouse
		if (bActive && SelectedCurveIndex >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f))
		{
			const FCurveHandle& Curve = Curves[SelectedCurveIndex];
			const int32 Key = State.SelectedKey;
			const int32 Comp = State.SelectedComponent;

			if (State.Drag == FCurveGraphState::EDrag::Key)
			{
				float NewTime = MouseX;
				if (Key > 0) NewTime = std::max(NewTime, Curve.KeyTime(Key - 1));
				if (Key + 1 < Curve.NumKeys()) NewTime = std::min(NewTime, Curve.KeyTime(Key + 1));
				// Shift: change only the value
				if (!IO.KeyShift) Curve.SetKeyTime(Key, NewTime);
				Curve.SetKeyValue(Key, Comp, MouseY);
				Curve.AutoSetTangents();
				EditedCurve = SelectedCurveIndex;
			}
			else if (State.Drag == FCurveGraphState::EDrag::ArriveTangent || State.Drag == FCurveGraphState::EDrag::LeaveTangent)
			{
				const bool bArrive = State.Drag == FCurveGraphState::EDrag::ArriveTangent;
				const ImVec2 Key2D = Transform.ToScreen(Curve.KeyTime(Key), Curve.KeyValue(Key, Comp));
				float OffsetX = Mouse.x - Key2D.x;
				const float OffsetY = Mouse.y - Key2D.y;
				// The arrive handle stays left of the key and the leave handle right, so the slope never flips through vertical.
				OffsetX = bArrive ? std::min(OffsetX, -1.0f) : std::max(OffsetX, 1.0f);
				const float NewTangent = (-OffsetY / Transform.ScaleY()) / (OffsetX / Transform.ScaleX());

				// Like UE: dragging an Auto key's handle turns it into a user key. Break keeps the sides separate.
				if (Curve.KeyMode(Key) == EInterpCurveMode::CurveBreak)
				{
					Curve.SetTangent(Key, Comp, bArrive, NewTangent);
				}
				else
				{
					Curve.SetKeyMode(Key, EInterpCurveMode::CurveUser);
					Curve.SetTangent(Key, Comp, true, NewTangent);
					Curve.SetTangent(Key, Comp, false, NewTangent);
				}
				EditedCurve = SelectedCurveIndex;
			}
		}

		// Pan: right or middle drag
		if (bActive && (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 3.0f) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 1.0f)))
		{
			const float DX = IO.MouseDelta.x / Transform.ScaleX();
			const float DY = IO.MouseDelta.y / Transform.ScaleY();
			State.ViewMinX -= DX; State.ViewMaxX -= DX;
			State.ViewMinY += DY; State.ViewMaxY += DY;
			State.Drag = FCurveGraphState::EDrag::Pan;
		}
		if (State.Drag == FCurveGraphState::EDrag::Pan && !ImGui::IsMouseDown(ImGuiMouseButton_Right) && !ImGui::IsMouseDown(ImGuiMouseButton_Middle))
		{
			State.Drag = FCurveGraphState::EDrag::None;
		}

		// Zoom around the mouse. Shift = X only, Ctrl = Y only.
		if (bHovered && IO.MouseWheel != 0.0f)
		{
			const float Factor = std::pow(0.85f, IO.MouseWheel);
			if (!IO.KeyCtrl)
			{
				State.ViewMinX = MouseX + (State.ViewMinX - MouseX) * Factor;
				State.ViewMaxX = MouseX + (State.ViewMaxX - MouseX) * Factor;
			}
			if (!IO.KeyShift)
			{
				State.ViewMinY = MouseY + (State.ViewMinY - MouseY) * Factor;
				State.ViewMaxY = MouseY + (State.ViewMaxY - MouseY) * Factor;
			}
			// Keep the range usable at extreme zoom
			if (State.ViewMaxX - State.ViewMinX < 1e-3f) State.ViewMaxX = State.ViewMinX + 1e-3f;
			if (State.ViewMaxY - State.ViewMinY < 1e-3f) State.ViewMaxY = State.ViewMinY + 1e-3f;
		}

		// Right-click (without dragging) on a key: mode / delete menu
		if (bHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && IO.MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] < 9.0f && HoverCurve >= 0)
		{
			State.MenuCurve = Curves[HoverCurve].Name();
			State.MenuKey = HoverKey;
			State.SelectedCurve = State.MenuCurve;
			State.SelectedKey = HoverKey;
			State.SelectedComponent = HoverComp;
			ImGui::OpenPopup("##CurveKeyMenu");
		}

		// Delete: remove the selected key (a curve keeps at least one)
		if ((bHovered || ImGui::IsWindowFocused()) && !IO.WantTextInput && SelectedCurveIndex >= 0 && ImGui::IsKeyPressed(ImGuiKey_Delete, false)
			&& Curves[SelectedCurveIndex].NumKeys() > 1)
		{
			Curves[SelectedCurveIndex].RemoveKey(State.SelectedKey);
			State.SelectedKey = -1;
			EditedCurve = SelectedCurveIndex;
			SelectedCurveIndex = -1;
			bShowHandles = false;
		}

		// ---- Keys and handles (drawn last so they sit on top of the lines) ----
		if (bShowHandles && SelectedCurveIndex >= 0)
		{
			// Re-read after this frame's edits
			const FCurveHandle& Curve = Curves[SelectedCurveIndex];
			const int32 Key = State.SelectedKey;
			const int32 Comp = State.SelectedComponent;
			KeyScreen = Transform.ToScreen(Curve.KeyTime(Key), Curve.KeyValue(Key, Comp));
			const ImVec2 ArriveOffset = TangentHandleOffset(Transform, Curve.Tangent(Key, Comp, true), true);
			const ImVec2 LeaveOffset = TangentHandleOffset(Transform, Curve.Tangent(Key, Comp, false), false);
			ArriveScreen = ImVec2(KeyScreen.x + ArriveOffset.x, KeyScreen.y + ArriveOffset.y);
			LeaveScreen = ImVec2(KeyScreen.x + LeaveOffset.x, KeyScreen.y + LeaveOffset.y);
			const ImU32 HandleColor = IM_COL32(200, 200, 120, 255);
			DrawList->AddLine(ArriveScreen, LeaveScreen, HandleColor, 1.0f);
			DrawList->AddCircleFilled(ArriveScreen, IsNear(ArriveScreen) ? 5.0f : 4.0f, HandleColor);
			DrawList->AddCircleFilled(LeaveScreen, IsNear(LeaveScreen) ? 5.0f : 4.0f, HandleColor);
		}

		for (int32 CurveIndex = 0; CurveIndex < Curves.Num(); ++CurveIndex)
		{
			const FCurveHandle& Curve = Curves[CurveIndex];
			for (int32 Key = 0; Key < Curve.NumKeys(); ++Key)
			{
				for (int32 Comp = 0; Comp < Curve.NumComponents(); ++Comp)
				{
					if (!IsComponentVisible(State, Curve, Comp)) continue;
					const ImVec2 P = Transform.ToScreen(Curve.KeyTime(Key), Curve.KeyValue(Key, Comp));
					const bool bSelected = CurveIndex == SelectedCurveIndex && Key == State.SelectedKey && Comp == State.SelectedComponent;
					const bool bHover = CurveIndex == HoverCurve && Key == HoverKey && Comp == HoverComp;
					const float Half = bSelected || bHover ? 5.0f : 4.0f;
					DrawList->AddRectFilled(ImVec2(P.x - Half, P.y - Half), ImVec2(P.x + Half, P.y + Half),
						bSelected ? IM_COL32(255, 255, 255, 255) : CurveComponentColor(Curve, Comp));
					DrawList->AddRect(ImVec2(P.x - Half, P.y - Half), ImVec2(P.x + Half, P.y + Half), IM_COL32(20, 20, 20, 255));
				}
			}
		}

		DrawList->PopClipRect();
		DrawList->AddRect(CanvasMin, CanvasMax, IM_COL32(70, 70, 75, 255));

		// Value readout for the hovered key, or the selected one while dragging
		if (HoverCurve >= 0 && State.Drag == FCurveGraphState::EDrag::None)
		{
			const FCurveHandle& Curve = Curves[HoverCurve];
			ImGui::SetTooltip("%s%s%s\nTime %.3f  Value %.3f\n%s", Curve.Name().c_str(), Curve.NumComponents() > 1 ? "." : "",
				Curve.NumComponents() > 1 ? ComponentLabel(HoverComp) : "",
				Curve.KeyTime(HoverKey), Curve.KeyValue(HoverKey, HoverComp), InterpCurveModeToName(Curve.KeyMode(HoverKey)));
		}
		else if (SelectedCurveIndex >= 0 && State.Drag != FCurveGraphState::EDrag::None && State.Drag != FCurveGraphState::EDrag::Pan)
		{
			const FCurveHandle& Curve = Curves[SelectedCurveIndex];
			ImGui::SetTooltip("Time %.3f  Value %.3f", Curve.KeyTime(State.SelectedKey), Curve.KeyValue(State.SelectedKey, State.SelectedComponent));
		}

		// ---- Key menu ----
		if (ImGui::BeginPopup("##CurveKeyMenu"))
		{
			const int32 MenuCurveIndex = FindCurve(Curves, State.MenuCurve);
			if (MenuCurveIndex >= 0 && State.MenuKey >= 0 && State.MenuKey < Curves[MenuCurveIndex].NumKeys())
			{
				const FCurveHandle& Curve = Curves[MenuCurveIndex];
				ImGui::TextDisabled("Segment to next key");
				for (int32 ModeIndex = 0; ModeIndex <= static_cast<int32>(EInterpCurveMode::Constant); ++ModeIndex)
				{
					const EInterpCurveMode Mode = static_cast<EInterpCurveMode>(ModeIndex);
					if (ImGui::MenuItem(InterpCurveModeToName(Mode), nullptr, Curve.KeyMode(State.MenuKey) == Mode))
					{
						Curve.SetKeyMode(State.MenuKey, Mode);
						Curve.AutoSetTangents();
						EditedCurve = MenuCurveIndex;
					}
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Delete key", "Del", false, Curve.NumKeys() > 1))
				{
					Curve.RemoveKey(State.MenuKey);
					State.SelectedKey = -1;
					EditedCurve = MenuCurveIndex;
				}
			}
			else
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		return EditedCurve;
	}
}

FParticleCurveEditorPanel::FParticleCurveEditorPanel(FParticleSystemEditor& InEditor)
	: FParticleSystemEditorPanel(InEditor, "Curve Editor", PanelIdPrefix)
{
}

void FParticleCurveEditorPanel::DrawContent()
{
	UParticleModule* Module = Cast<UParticleModule>(Editor.GetSelectedObject());
	if (!Module)
	{
		Graph.LastObject = nullptr;
		ImGui::TextDisabled("Select a module. Over-life modules always have curves; other values show here when their type is Curve.");
		return;
	}

	// New module: fresh view and selection
	if (Graph.LastObject != Module)
	{
		Graph = FCurveGraphState();
		Graph.LastObject = Module;
	}

	// Every curve property of the selected module, found through reflection, so new curve modules need no panel changes.
	const TArray<FCurveHandle> Curves = GatherCurves(Module);
	if (Curves.IsEmpty())
	{
		ImGui::TextDisabled("%s has no curves. Set a value's type to Curve in Details to edit it here.", GetModuleDisplayName(Module).c_str());
		return;
	}

	// The module decides what X means (it is what it passes to Eval). All current modules use one axis for every curve.
	bool bAllRelativeTime = true;
	bool bAllEmitterTime = true;
	for (const FCurveHandle& Curve : Curves)
	{
		const EParticleCurveInput Input = Module->GetCurveInput(*Curve.Property);
		bAllRelativeTime &= Input == EParticleCurveInput::RelativeTime;
		bAllEmitterTime &= Input == EParticleCurveInput::EmitterTime;
	}
	const char* AxisHint = bAllRelativeTime
		? "Time = particle RelativeTime (0 at spawn, 1 at death)."
		: bAllEmitterTime
			? "Time = emitter loop (0 at loop start, 1 at EmitterDuration). Read when each particle spawns."
			: "Time = 0..1 (RelativeTime or emitter loop, per curve).";

	ImGui::PushID(Module);
	const auto NotifyEdited = [this, Module](const FCurveHandle& Curve)
	{
		Module->PostEditChangeProperty(*Curve.Property);
		Editor.NotifyPropertyEdited();
	};

	const int32 GraphEdited = DrawCurveGraph(Graph, Curves);
	if (GraphEdited >= 0)
	{
		NotifyEdited(Curves[GraphEdited]);
	}
	ImGui::TextDisabled("%s Double-click: add key, right-click key: mode.", AxisHint);

	// Exact numbers: the same curves as a key table
	if (ImGui::CollapsingHeader("Key Table"))
	{
		for (const FCurveHandle& Curve : Curves)
		{
			const bool bEdited = Curve.FloatCurve
				? DrawCurveSection(Curve.Name(), *Curve.FloatCurve, 0.0f)
				: DrawCurveSection(Curve.Name(), *Curve.VectorCurve, FVector::OneVector);
			if (bEdited)
			{
				NotifyEdited(Curve);
			}
		}
	}
	ImGui::PopID();
}
