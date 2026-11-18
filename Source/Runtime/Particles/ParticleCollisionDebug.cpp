#include "EnginePCH.h"
#include "Particles/ParticleCollisionDebug.h"
#include "Render/LineBatcher.h"
#include <chrono>

bool GShowParticleCollisionDebug = true;

namespace
{
	/** Swept/후보 박스는 최근 이 시간 안의 것만 그린다 (= 거의 "이번 프레임") */
	constexpr double FrameEntryLifetime = 0.1;
	/** 충돌 지점은 이 시간 동안 남겨 두고 점점 흐리게 한다 */
	constexpr double HitLifetime = 1.5;
	/** 메모리 상한 (파티클이 아주 많아도 기록이 무한히 늘지 않게) */
	constexpr int32 MaxHits = 2048;
	constexpr int32 MaxFrameEntries = 256;

	constexpr float HitNormalLength = 0.5f;
	constexpr float HitMarkerSize = 0.08f;

	struct FFrameEntry
	{
		UWorld* World = nullptr;
		double Time = 0.0;
		FBox SweptBox;
		TArray<FBox> Candidates;
	};

	struct FHitEntry
	{
		UWorld* World = nullptr;
		double Time = 0.0;
		FVector Location;
		FVector Normal;
	};

	TArray<FFrameEntry> GFrameEntries;
	TArray<FHitEntry> GHits;
	int32 GTotalHits = 0;

	double NowSeconds()
	{
		using namespace std::chrono;
		static const steady_clock::time_point Start = steady_clock::now();
		return duration<double>(steady_clock::now() - Start).count();
	}

	template <typename T>
	void PruneOlderThan(TArray<T>& Entries, double MinTime)
	{
		// 시간순으로 쌓이므로 앞에서부터 오래된 것만 잘라낸다
		int32 FirstAlive = 0;
		while (FirstAlive < Entries.Num() && Entries[FirstAlive].Time < MinTime)
		{
			++FirstAlive;
		}
		if (FirstAlive > 0)
		{
			Entries.RemoveAt(0, FirstAlive);
		}
	}

	void DrawCross(FLineBatcher* LineBatcher, const FVector& P, float S, const FVector4& Color)
	{
		LineBatcher->AddLine(P - FVector(S, 0, 0), P + FVector(S, 0, 0), Color);
		LineBatcher->AddLine(P - FVector(0, S, 0), P + FVector(0, S, 0), Color);
		LineBatcher->AddLine(P - FVector(0, 0, S), P + FVector(0, 0, S), Color);
	}
}

namespace ParticleCollisionDebug
{
	void RecordFrame(UWorld* World, const FBox& SweptBox, const TArray<FBox>& CandidateBounds)
	{
		if (!GShowParticleCollisionDebug)
		{
			return;
		}

		const double Now = NowSeconds();
		PruneOlderThan(GFrameEntries, Now - FrameEntryLifetime);
		if (GFrameEntries.Num() >= MaxFrameEntries)
		{
			GFrameEntries.RemoveAt(0);
		}

		FFrameEntry Entry;
		Entry.World = World;
		Entry.Time = Now;
		Entry.SweptBox = SweptBox;
		Entry.Candidates = CandidateBounds;
		GFrameEntries.Add(std::move(Entry));
	}

	void RecordHit(UWorld* World, const FVector& Location, const FVector& Normal)
	{
		++GTotalHits;
		if (!GShowParticleCollisionDebug)
		{
			return;
		}

		const double Now = NowSeconds();
		if (GHits.Num() >= MaxHits)
		{
			PruneOlderThan(GHits, Now - HitLifetime);
			if (GHits.Num() >= MaxHits)
			{
				GHits.RemoveAt(0, MaxHits / 4);
			}
		}
		GHits.Add({ World, Now, Location, Normal });
	}

	void Draw(UWorld* World, FLineBatcher* LineBatcher)
	{
		if (!GShowParticleCollisionDebug || !World || !LineBatcher)
		{
			return;
		}

		const double Now = NowSeconds();
		PruneOlderThan(GFrameEntries, Now - FrameEntryLifetime);
		PruneOlderThan(GHits, Now - HitLifetime);

		// 노랑: 이미터가 이번 프레임에 지나간 전체 영역(Swept AABB)
		// 주황: 그 영역과 겹쳐 실제로 검사한 후보 컴포넌트 바운드
		const FVector4 SweptColor(1.0f, 1.0f, 0.0f, 1.0f);
		const FVector4 CandidateColor(1.0f, 0.5f, 0.0f, 1.0f);
		for (const FFrameEntry& Entry : GFrameEntries)
		{
			if (Entry.World != World)
			{
				continue;
			}
			LineBatcher->AddBox(Entry.SweptBox, SweptColor);
			for (const FBox& Candidate : Entry.Candidates)
			{
				LineBatcher->AddBox(Candidate, CandidateColor);
			}
		}

		// 빨강 X: 충돌 지점, 초록 선: 그 면의 법선. 오래될수록 어두워진다
		for (const FHitEntry& Hit : GHits)
		{
			if (Hit.World != World)
			{
				continue;
			}
			const float Fade = 1.0f - static_cast<float>((Now - Hit.Time) / HitLifetime);
			DrawCross(LineBatcher, Hit.Location, HitMarkerSize, FVector4(Fade, 0.1f * Fade, 0.1f * Fade, 1.0f));
			LineBatcher->AddLine(Hit.Location, Hit.Location + Hit.Normal * HitNormalLength,
				FVector4(0.1f * Fade, Fade, 0.1f * Fade, 1.0f));
		}
	}

	FStats GetStats()
	{
		const double Now = NowSeconds();

		FStats Stats;
		Stats.TotalHits = GTotalHits;

		// "최근 프레임" = 가장 마지막 기록 시각과 거의 같은 시각의 기록들
		const double LatestFrame = GFrameEntries.IsEmpty() ? -1.0 : GFrameEntries.Last().Time;
		for (const FFrameEntry& Entry : GFrameEntries)
		{
			if (LatestFrame - Entry.Time < 0.008)
			{
				++Stats.TestedEmitters;
				Stats.Candidates += Entry.Candidates.Num();
			}
		}
		for (const FHitEntry& Hit : GHits)
		{
			if (Now - Hit.Time <= 1.0)
			{
				++Stats.HitsPerSecond;
			}
		}
		return Stats;
	}

	void Reset()
	{
		GFrameEntries.Reset();
		GHits.Reset();
		GTotalHits = 0;
	}
}
