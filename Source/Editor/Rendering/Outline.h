#pragma once

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

class FOutlineRenderer;
struct FSceneView;
struct FViewportSettings;

class FOutline
{
public:
	FOutline();
	~FOutline();

	// 대상 메시의 외곽선을 View의 카메라 기준으로 그린다. 씬 위에 올리는 패스 안에서 부른다.
	// FGizmo::Render와 같은 방식이다. UE는 선택 외곽선을 렌더러 내부에서 그리므로 대응하는 함수가 없다.
	void Render(const FSceneView* View, const FViewportSettings& Viewport) const;

	// 컴포넌트 하나만 외곽선 대상으로 둔다 (nullptr이면 선택 해제)
	void SetTarget(UPrimitiveComponent* InTarget)
	{
		TargetActor = nullptr;
		Targets.Empty();
		if (InTarget) Targets.Add(InTarget);
	}
	// 액터를 외곽선 대상으로 둔다. 그 액터의 Primitive 컴포넌트 전부에 외곽선을 그린다
	// (Unreal도 액터 선택 시 소유한 Primitive 컴포넌트가 모두 선택 표시된다).
	// 컴포넌트 목록은 그릴 때마다 액터에서 다시 모으므로, 선택 뒤에 추가·삭제된 컴포넌트도 바로 반영된다.
	void SetTargetActor(AActor* InActor)
	{
		TargetActor = InActor;
		Targets.Empty();
	}
	// 이번에 그릴 대상 목록. 액터가 대상이면 액터의 현재 컴포넌트로 다시 채운다.
	const TArray<UPrimitiveComponent*>& GetTargets() const
	{
		if (TargetActor)
		{
			Targets.Empty();
			for (UActorComponent* Component : TargetActor->GetComponents())
				if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
					Targets.Add(Primitive);
		}
		return Targets;
	}
	bool HasTarget() const { return TargetActor != nullptr || !Targets.IsEmpty(); }
	UPrimitiveComponent* GetTarget() const
	{
		const TArray<UPrimitiveComponent*>& Current = GetTargets();
		return Current.IsEmpty() ? nullptr : Current[0];
	}
private:
	AActor* TargetActor = nullptr;
	mutable TArray<UPrimitiveComponent*> Targets;

	// 스텐실과 셰이더로 실제 그리기를 맡는 도구
	TUniquePtr<FOutlineRenderer> Renderer;

};