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
#include <cstring>
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
		{ "UParticleModuleTypeDataMesh",      "Mesh Data",        "TypeData" },
		{ "UParticleModuleTypeDataRibbon",    "Ribbon Data",      "TypeData" },
		{ "UParticleModuleTypeDataBeam2",     "Beam Data",        "TypeData" },
		{ "UParticleModuleBeamTarget",        "Beam Target",      "Beam" },
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

FParticleCurveEditorPanel::FParticleCurveEditorPanel(FParticleSystemEditor& InEditor)
	: FParticleSystemEditorPanel(InEditor, "Curve Editor", PanelIdPrefix)
{
}

void FParticleCurveEditorPanel::DrawContent()
{
	ImGui::TextDisabled("Distribution curves will be edited here.");
}
