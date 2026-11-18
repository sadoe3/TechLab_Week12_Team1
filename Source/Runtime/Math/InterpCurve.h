#pragma once

#include <algorithm>

#include "Containers/Array.h"
#include "Core/EngineString.h"
#include "Math/MathUtility.h"
#include "Math/Vector.h"
#include "Serialization/StructuredArchiveSlots.h"
#include "Serialization/MathSerialization.h"

// Keyframed curve stored inline by value (UE: Math/InterpCurve.h, FInterpCurveFloat / FInterpCurveVector).
// Particle modules use it for "value over X" properties, e.g. ColorOverLife (X = particle RelativeTime 0..1).
//
// UCurveFloat is a standalone asset with stable key ids, and FMovieSceneFloatChannel works in frame ticks.
// A particle curve is part of a module, is copied with it, and is evaluated per particle per frame, so it gets its own light type.

// How the segment from this point to the next one is shaped (UE: EInterpCurveMode, same meaning)
enum class EInterpCurveMode : uint8
{
	Linear,           // straight line to the next point
	CurveAuto,        // smooth; tangents recomputed from neighbours (Catmull-Rom)
	CurveAutoClamped, // like CurveAuto, but flat at local min/max so it never overshoots
	CurveUser,        // smooth; Arrive = Leave tangent set by the user
	CurveBreak,       // smooth; Arrive and Leave tangents set separately
	Constant,         // hold this value until the next point
};

inline const char* InterpCurveModeToName(EInterpCurveMode Mode)
{
	switch (Mode)
	{
	case EInterpCurveMode::Linear:           return "Linear";
	case EInterpCurveMode::CurveAuto:        return "CurveAuto";
	case EInterpCurveMode::CurveAutoClamped: return "CurveAutoClamped";
	case EInterpCurveMode::CurveUser:        return "CurveUser";
	case EInterpCurveMode::CurveBreak:       return "CurveBreak";
	case EInterpCurveMode::Constant:         return "Constant";
	}
	return "Linear";
}

inline bool InterpCurveModeFromName(const FString& Name, EInterpCurveMode& OutMode)
{
	for (int32 i = 0; i <= static_cast<int32>(EInterpCurveMode::Constant); ++i)
	{
		const EInterpCurveMode Mode = static_cast<EInterpCurveMode>(i);
		if (Name == InterpCurveModeToName(Mode))
		{
			OutMode = Mode;
			return true;
		}
	}
	return false;
}

inline bool IsInterpCurveModeCurve(EInterpCurveMode Mode)
{
	return Mode != EInterpCurveMode::Linear && Mode != EInterpCurveMode::Constant;
}

// Per-component access, so float and FVector curves share one implementation (and the editor can draw each component).
template <typename T>
struct TInterpCurveTraits;

template <>
struct TInterpCurveTraits<float>
{
	static constexpr int32 NumComponents = 1;
	static float& Component(float& Value, int32) { return Value; }
	static float Component(const float& Value, int32) { return Value; }
	static float Zero() { return 0.0f; }
};

template <>
struct TInterpCurveTraits<FVector>
{
	static constexpr int32 NumComponents = 3;
	static float& Component(FVector& Value, int32 Index) { return Value.V[Index]; }
	static float Component(const FVector& Value, int32 Index) { return Value.V[Index]; }
	static FVector Zero() { return FVector(0.0f, 0.0f, 0.0f); }
};

// One key (UE: FInterpCurvePoint). Tangents are in "value per unit of InVal".
template <typename T>
struct FInterpCurvePoint
{
	float InVal = 0.0f;
	T OutVal = TInterpCurveTraits<T>::Zero();
	T ArriveTangent = TInterpCurveTraits<T>::Zero();
	T LeaveTangent = TInterpCurveTraits<T>::Zero();
	EInterpCurveMode InterpMode = EInterpCurveMode::Linear;

	FInterpCurvePoint() = default;
	FInterpCurvePoint(float InInVal, const T& InOutVal, EInterpCurveMode InMode = EInterpCurveMode::Linear)
		: InVal(InInVal), OutVal(InOutVal), InterpMode(InMode) {}

	bool IsCurveKey() const { return IsInterpCurveModeCurve(InterpMode); }
};

// Points are always kept sorted by InVal. Before the first / after the last point the curve holds the end value.
template <typename T>
struct FInterpCurve
{
	using ValueType = T;
	using FPoint = FInterpCurvePoint<T>;
	using FTraits = TInterpCurveTraits<T>;

	TArray<FPoint> Points;

	FInterpCurve() = default;

	// 2 linear points at 0 and 1: the same result as the old Start -> End lerp.
	static FInterpCurve MakeLinear(const T& Start, const T& End)
	{
		FInterpCurve Curve;
		Curve.Points.Add(FPoint(0.0f, Start));
		Curve.Points.Add(FPoint(1.0f, End));
		return Curve;
	}

	static FInterpCurve MakeConstant(const T& Value)
	{
		FInterpCurve Curve;
		Curve.Points.Add(FPoint(0.0f, Value));
		return Curve;
	}

	int32 Num() const { return Points.Num(); }

	// Inserts a point keeping the sort order and returns its index. A point at the same InVal goes after it.
	int32 AddPoint(float InVal, const T& OutVal, EInterpCurveMode Mode = EInterpCurveMode::Linear)
	{
		int32 Index = 0;
		while (Index < Points.Num() && Points[Index].InVal <= InVal)
		{
			++Index;
		}
		Points.Insert(FPoint(InVal, OutVal, Mode), static_cast<uint32>(Index));
		AutoSetTangents();
		return Index;
	}

	void RemovePoint(int32 Index)
	{
		if (Index >= 0 && Index < Points.Num())
		{
			Points.RemoveAt(static_cast<uint32>(Index));
			AutoSetTangents();
		}
	}

	// Changes a point's InVal, re-sorts, and returns where the point ended up (UE: FInterpCurve::MovePoint)
	int32 MovePoint(int32 Index, float NewInVal)
	{
		if (Index < 0 || Index >= Points.Num())
		{
			return -1;
		}
		FPoint Point = Points[Index];
		Points.RemoveAt(static_cast<uint32>(Index));
		Point.InVal = NewInVal;

		int32 NewIndex = 0;
		while (NewIndex < Points.Num() && Points[NewIndex].InVal <= NewInVal)
		{
			++NewIndex;
		}
		Points.Insert(Point, static_cast<uint32>(NewIndex));
		AutoSetTangents();
		return NewIndex;
	}

	// Value at InVal. An empty curve returns Default.
	T Eval(float InVal, const T& Default = TInterpCurveTraits<T>::Zero()) const
	{
		const int32 NumPoints = Points.Num();
		if (NumPoints == 0)
		{
			return Default;
		}
		if (NumPoints == 1 || InVal <= Points[0].InVal)
		{
			return Points[0].OutVal;
		}
		if (InVal >= Points[NumPoints - 1].InVal)
		{
			return Points[NumPoints - 1].OutVal;
		}

		// Few keys per particle curve: a linear scan beats binary search here.
		int32 Index = 0;
		while (Index + 1 < NumPoints && Points[Index + 1].InVal <= InVal)
		{
			++Index;
		}
		return EvalSegment(Points[Index], Points[Index + 1], InVal);
	}

	// Recomputes tangents of CurveAuto / CurveAutoClamped points from their neighbours (UE: AutoSetTangents).
	// End points get flat tangents, so a curve starts and ends at rest.
	void AutoSetTangents(float Tension = 0.0f)
	{
		const int32 NumPoints = Points.Num();
		for (int32 Index = 0; Index < NumPoints; ++Index)
		{
			FPoint& Point = Points[Index];
			const bool bAuto = Point.InterpMode == EInterpCurveMode::CurveAuto;
			const bool bClamped = Point.InterpMode == EInterpCurveMode::CurveAutoClamped;
			if (!bAuto && !bClamped)
			{
				continue;
			}

			T Tangent = FTraits::Zero();
			if (Index > 0 && Index < NumPoints - 1)
			{
				const FPoint& Prev = Points[Index - 1];
				const FPoint& Next = Points[Index + 1];
				const float TimeDiff = std::max(KINDA_SMALL_NUMBER, Next.InVal - Prev.InVal);
				for (int32 C = 0; C < FTraits::NumComponents; ++C)
				{
					const float P = FTraits::Component(Prev.OutVal, C);
					const float V = FTraits::Component(Point.OutVal, C);
					const float N = FTraits::Component(Next.OutVal, C);
					// Clamped: a local min/max stays flat so the curve does not overshoot past the key.
					const bool bExtremum = (V >= P && V >= N) || (V <= P && V <= N);
					FTraits::Component(Tangent, C) = (bClamped && bExtremum) ? 0.0f : (1.0f - Tension) * (N - P) / TimeDiff;
				}
			}
			Point.ArriveTangent = Tangent;
			Point.LeaveTangent = Tangent;
		}
	}

	// Smallest / largest InVal and component value over all points (for fitting the editor graph)
	void GetInRange(float& OutMin, float& OutMax) const
	{
		if (Points.Num() == 0)
		{
			OutMin = 0.0f;
			OutMax = 0.0f;
			return;
		}
		OutMin = Points[0].InVal;
		OutMax = Points[Points.Num() - 1].InVal;
	}

	void GetOutRange(float& OutMin, float& OutMax) const
	{
		OutMin = 0.0f;
		OutMax = 0.0f;
		bool bFirst = true;
		for (const FPoint& Point : Points)
		{
			for (int32 C = 0; C < FTraits::NumComponents; ++C)
			{
				const float V = FTraits::Component(Point.OutVal, C);
				OutMin = bFirst ? V : std::min(OutMin, V);
				OutMax = bFirst ? V : std::max(OutMax, V);
				bFirst = false;
			}
		}
	}

private:
	static T EvalSegment(const FPoint& Prev, const FPoint& Next, float InVal)
	{
		const float Diff = Next.InVal - Prev.InVal;
		if (Diff <= 0.0f || Prev.InterpMode == EInterpCurveMode::Constant)
		{
			return Prev.OutVal;
		}

		const float Alpha = (InVal - Prev.InVal) / Diff;
		if (Prev.InterpMode == EInterpCurveMode::Linear)
		{
			return Prev.OutVal + (Next.OutVal - Prev.OutVal) * Alpha;
		}

		// Cubic Hermite (UE: FMath::CubicInterp). Tangents are per unit InVal, so scale them by the segment length.
		const float A2 = Alpha * Alpha;
		const float A3 = A2 * Alpha;
		const float H00 = 2.0f * A3 - 3.0f * A2 + 1.0f;
		const float H10 = A3 - 2.0f * A2 + Alpha;
		const float H01 = -2.0f * A3 + 3.0f * A2;
		const float H11 = A3 - A2;
		return Prev.OutVal * H00 + Prev.LeaveTangent * (H10 * Diff) + Next.OutVal * H01 + Next.ArriveTangent * (H11 * Diff);
	}
};

using FInterpCurveFloat = FInterpCurve<float>;
using FInterpCurveVector = FInterpCurve<FVector>;

// json: { "Points": [ { "InVal": 0, "OutVal": ..., "ArriveTangent": ..., "LeaveTangent": ..., "InterpMode": "Linear" }, ... ] }
template <typename T>
void operator<<(FStructuredArchiveSlot Slot, FInterpCurve<T>& Curve)
{
	const bool bLoading = Slot.GetUnderlyingArchive().IsLoading();
	FStructuredArchiveRecord Record = Slot.EnterRecord();

	int32 NumPoints = Curve.Points.Num();
	FStructuredArchiveArray PointArray = Record.EnterArray("Points", NumPoints);
	if (bLoading)
	{
		Curve.Points.SetNum(NumPoints);
	}

	for (int32 Index = 0; Index < NumPoints; ++Index)
	{
		FInterpCurvePoint<T>& Point = Curve.Points[Index];
		FStructuredArchiveRecord PointRecord = PointArray.EnterElement().EnterRecord();
		FString ModeName = InterpCurveModeToName(Point.InterpMode);
		PointRecord << SA_VALUE("InVal", Point.InVal);
		PointRecord << SA_VALUE("OutVal", Point.OutVal);
		PointRecord << SA_VALUE("ArriveTangent", Point.ArriveTangent);
		PointRecord << SA_VALUE("LeaveTangent", Point.LeaveTangent);
		PointRecord << SA_VALUE("InterpMode", ModeName);
		if (bLoading && !InterpCurveModeFromName(ModeName, Point.InterpMode))
		{
			Point.InterpMode = EInterpCurveMode::Linear;
		}
	}

	if (bLoading)
	{
		// A hand-edited file may be out of order. Eval assumes sorted points.
		std::stable_sort(Curve.Points.begin(), Curve.Points.end(),
			[](const FInterpCurvePoint<T>& A, const FInterpCurvePoint<T>& B) { return A.InVal < B.InVal; });
		Curve.AutoSetTangents();
	}
}
