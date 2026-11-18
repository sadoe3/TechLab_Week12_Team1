#include "EnginePCH.h"
#include "ParticleSystemEditorTab.h"
#include "ParticleSystemEditor.h"
#include "Panels/ParticleSystemEditorPanels.h"

namespace
{
	enum EParticlePanel : int32
	{
		Panel_Viewport = 0,
		Panel_Emitters,
		Panel_Details,
		Panel_CurveEditor,
	};

	constexpr const char* UnsavedPopupName = "Unsaved Particle System Changes";
	constexpr const char* AboutPopupName = "About Particle System Editor";
}

FParticleSystemEditorTab::FParticleSystemEditorTab(FParticleSystemEditor& InEditor)
	: FDockedAssetEditorTab(InEditor, "ParticleSystemDockSpace", "ParticleSystemPanelClass")
	, ParticleEditor(InEditor)
{
	// 순서는 EParticlePanel과 같아야 한다.
	RegisterPanel(MakeUnique<FParticleViewportPanel>(InEditor));
	RegisterPanel(MakeUnique<FParticleEmittersPanel>(InEditor));
	RegisterPanel(MakeUnique<FParticleDetailsPanel>(InEditor));
	RegisterPanel(MakeUnique<FParticleCurveEditorPanel>(InEditor));
}

void FParticleSystemEditorTab::DrawMenuBar()
{
	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("Save", "Ctrl+S"))
		{
			ParticleEditor.Save();
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Close Editor"))
		{
			ParticleEditor.Close();
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Edit"))
	{
		// Undo/Redo는 트랜잭션 시스템이 생기면 연결한다.
		ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
		ImGui::MenuItem("Redo", "Ctrl+Y", false, false);
		ImGui::Separator();
		if (ImGui::MenuItem("Restart Simulation", "Space"))
		{
			ParticleEditor.RestartSimulation();
		}
		if (ImGui::MenuItem("Pause Simulation", nullptr, ParticleEditor.IsPaused()))
		{
			ParticleEditor.SetPaused(!ParticleEditor.IsPaused());
		}
		if (ImGui::MenuItem("Auto Restart", nullptr, ParticleEditor.IsAutoRestart()))
		{
			ParticleEditor.SetAutoRestart(!ParticleEditor.IsAutoRestart());
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Window"))
	{
		for (auto& Panel : Panels)
		{
			if (ImGui::MenuItem(Panel->GetPanelName(), nullptr, Panel->IsOpen()))
			{
				Panel->SetOpen(!Panel->IsOpen());
			}
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Help"))
	{
		if (ImGui::MenuItem("About Particle System Editor"))
		{
			// 메뉴 안에서 연 팝업은 메뉴의 ID 스택에 묶이므로 탭 렌더링에서 연다.
			bAboutRequested = true;
		}
		ImGui::EndMenu();
	}
}

void FParticleSystemEditorTab::BeforeRender()
{
	// 패널이 그려지지 않은 프레임에는 프리뷰 입력을 비운다.
	ParticleEditor.SetViewportState(0, 0, {});
}

void FParticleSystemEditorTab::OnRender(bool bActive)
{
	if (bActive && !ImGui::GetIO().WantTextInput)
	{
		const ImGuiIO& IO = ImGui::GetIO();
		if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
		{
			ParticleEditor.Save();
		}
	}

	FDockedAssetEditorTab::OnRender(bActive);

	DrawUnsavedPrompt();
	DrawAboutPopup();
}

void FParticleSystemEditorTab::BuildDefaultLayout(ImGuiID InDockSpaceId, const ImVec2& Size)
{
	// Cascade 배치: 왼쪽 위 Viewport, 왼쪽 아래 Details, 오른쪽 위 Emitters, 오른쪽 아래 Curve Editor
	ImGui::DockBuilderAddNode(InDockSpaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(InDockSpaceId, Size);

	ImGuiID LeftId = InDockSpaceId;
	ImGuiID RightId = ImGui::DockBuilderSplitNode(LeftId, ImGuiDir_Right, 0.62f, nullptr, &LeftId);

	ImGuiID ViewportId = LeftId;
	const ImGuiID DetailsId = ImGui::DockBuilderSplitNode(ViewportId, ImGuiDir_Down, 0.45f, nullptr, &ViewportId);

	ImGuiID EmittersId = RightId;
	const ImGuiID CurveId = ImGui::DockBuilderSplitNode(EmittersId, ImGuiDir_Down, 0.45f, nullptr, &EmittersId);

	ImGui::DockBuilderDockWindow(Panels[Panel_Viewport]->GetWindowName(), ViewportId);
	ImGui::DockBuilderDockWindow(Panels[Panel_Emitters]->GetWindowName(), EmittersId);
	ImGui::DockBuilderDockWindow(Panels[Panel_Details]->GetWindowName(), DetailsId);
	ImGui::DockBuilderDockWindow(Panels[Panel_CurveEditor]->GetWindowName(), CurveId);
	ImGui::DockBuilderFinish(InDockSpaceId);
}

void FParticleSystemEditorTab::DrawUnsavedPrompt()
{
	if (ParticleEditor.IsUnsavedPromptRequested())
	{
		ParticleEditor.ClearUnsavedPromptRequest();
		ImGui::OpenPopup(UnsavedPopupName);
	}

	const ImGuiViewport* Viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(Viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal(UnsavedPopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("%s has unsaved changes.", ParticleEditor.GetAssetPath().c_str());
		ImGui::TextDisabled("Closing without saving keeps the edits in memory until the editor restarts.");
		ImGui::Spacing();

		if (ImGui::Button("Save and Close"))
		{
			if (ParticleEditor.Save())
			{
				ImGui::CloseCurrentPopup();
				ParticleEditor.CloseWithoutPrompt();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Close Without Saving"))
		{
			ImGui::CloseCurrentPopup();
			ParticleEditor.CloseWithoutPrompt();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
		}

		const FString& Status = ParticleEditor.GetStatusMessage();
		if (Status.rfind("Save failed", 0) == 0)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", Status.c_str());
		}
		ImGui::EndPopup();
	}
}

void FParticleSystemEditorTab::DrawAboutPopup()
{
	if (bAboutRequested)
	{
		bAboutRequested = false;
		ImGui::OpenPopup(AboutPopupName);
	}

	const ImGuiViewport* Viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(Viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal(AboutPopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Particle System Editor (Cascade style)");
		ImGui::Separator();
		ImGui::BulletText("Viewport: RMB + WASD/QE to fly, wheel to zoom, F to focus");
		ImGui::BulletText("Space: restart simulation");
		ImGui::BulletText("Emitters: click an emitter or module to edit it in Details");
		ImGui::BulletText("Ctrl+S: save the .particle asset");
		ImGui::Spacing();
		if (ImGui::Button("OK"))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}
