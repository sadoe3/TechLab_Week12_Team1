#pragma once

#include "Math/InterpCurve.h"

// A value that can be authored three ways (UE: FRawDistributionFloat / FRawDistributionVector,
// with UDistributionFloatConstant / Uniform / ConstantCurve behind it).
//
//   Constant : always the same value
//   Uniform  : random in [Min, Max], each component independently
//   Curve    : Curve.Eval(Time)
//
// The distribution does not know what Time means. The module that reads it decides (spawn-time modules pass
// the emitter's loop fraction, see FParticleEmitterInstance::GetEmitterTimeFraction), and the random source
// comes from the caller too, so this header stays plain data.

enum class EDistributionType : uint8
{
	Constant,
	Uniform,
	Curve,
};

inline const char* DistributionTypeToName(EDistributionType Type)
{
	switch (Type)
	{
	case EDistributionType::Constant: return "Constant";
	case EDistributionType::Uniform:  return "Uniform";
	case EDistributionType::Curve:    return "Curve";
	}
	return "Constant";
}

inline bool DistributionTypeFromName(const FString& Name, EDistributionType& OutType)
{
	for (int32 i = 0; i <= static_cast<int32>(EDistributionType::Curve); ++i)
	{
		const EDistributionType Type = static_cast<EDistributionType>(i);
		if (Name == DistributionTypeToName(Type))
		{
			OutType = Type;
			return true;
		}
	}
	return false;
}

template <typename T>
struct TDistribution
{
	using ValueType = T;
	using FTraits = TInterpCurveTraits<T>;

	EDistributionType Type = EDistributionType::Constant;
	T Constant = FTraits::Zero();
	T Min = FTraits::Zero();
	T Max = FTraits::Zero();
	FInterpCurve<T> Curve;

	static TDistribution MakeConstant(const T& Value)
	{
		TDistribution Distribution;
		Distribution.Type = EDistributionType::Constant;
		Distribution.Constant = Value;
		Distribution.Min = Value;
		Distribution.Max = Value;
		return Distribution;
	}

	static TDistribution MakeUniform(const T& InMin, const T& InMax)
	{
		TDistribution Distribution;
		Distribution.Type = EDistributionType::Uniform;
		Distribution.Constant = InMin;
		Distribution.Min = InMin;
		Distribution.Max = InMax;
		return Distribution;
	}

	// Value given a random number per component (0..1). Rand01 is only called for Uniform.
	template <typename RandomFuncType>
	T GetValue(float Time, RandomFuncType&& Rand01) const
	{
		switch (Type)
		{
		case EDistributionType::Uniform:
		{
			T Result = Min;
			for (int32 C = 0; C < FTraits::NumComponents; ++C)
			{
				const float Lo = FTraits::Component(Min, C);
				const float Hi = FTraits::Component(Max, C);
				FTraits::Component(Result, C) = Lo == Hi ? Lo : Lo + (Hi - Lo) * Rand01();
			}
			return Result;
		}
		case EDistributionType::Curve:
			return Curve.Eval(Time, Constant);
		case EDistributionType::Constant:
		default:
			return Constant;
		}
	}

	// Largest value each component can take (e.g. the longest possible Lifetime, for bounds)
	T GetMaxValue() const
	{
		switch (Type)
		{
		case EDistributionType::Uniform:
		{
			T Result = Min;
			for (int32 C = 0; C < FTraits::NumComponents; ++C)
			{
				FTraits::Component(Result, C) = std::max(FTraits::Component(Min, C), FTraits::Component(Max, C));
			}
			return Result;
		}
		case EDistributionType::Curve:
		{
			if (Curve.Num() == 0) return Constant;
			// Sample instead of reading keys: smooth segments can overshoot between keys.
			float InMin = 0.0f, InMax = 0.0f;
			Curve.GetInRange(InMin, InMax);
			T Result = Curve.Eval(InMin);
			constexpr int32 Samples = 32;
			for (int32 i = 1; i <= Samples; ++i)
			{
				const T Value = Curve.Eval(InMin + (InMax - InMin) * i / Samples);
				for (int32 C = 0; C < FTraits::NumComponents; ++C)
				{
					FTraits::Component(Result, C) = std::max(FTraits::Component(Result, C), FTraits::Component(Value, C));
				}
			}
			return Result;
		}
		case EDistributionType::Constant:
		default:
			return Constant;
		}
	}

	// Switches type and seeds the new type from the current value, so switching never jumps to zero.
	// An existing curve is kept, so toggling Curve -> Constant -> Curve does not lose the keys.
	void SetType(EDistributionType NewType)
	{
		if (NewType == Type) return;
		const T Representative = GetRepresentativeValue();
		switch (NewType)
		{
		case EDistributionType::Constant:
			Constant = Representative;
			break;
		case EDistributionType::Uniform:
			if (Type == EDistributionType::Constant || Type == EDistributionType::Curve)
			{
				Min = Representative;
				Max = Representative;
			}
			break;
		case EDistributionType::Curve:
			if (Curve.Num() == 0)
			{
				Curve = FInterpCurve<T>::MakeLinear(Representative, Representative);
			}
			break;
		}
		Type = NewType;
	}

private:
	T GetRepresentativeValue() const
	{
		switch (Type)
		{
		case EDistributionType::Uniform:  return Min + (Max - Min) * 0.5f;
		case EDistributionType::Curve:    return Curve.Eval(0.0f, Constant);
		case EDistributionType::Constant:
		default:                          return Constant;
		}
	}
};

using FDistributionFloat = TDistribution<float>;
using FDistributionVector = TDistribution<FVector>;

// json writes only what the type uses:
//   { "Type": "Constant", "Constant": 1.0 }
//   { "Type": "Uniform", "Min": 0.5, "Max": 2.0 }
//   { "Type": "Curve", "Curve": { "Points": [...] } }
template <typename T>
void operator<<(FStructuredArchiveSlot Slot, TDistribution<T>& Distribution)
{
	const bool bLoading = Slot.GetUnderlyingArchive().IsLoading();
	FStructuredArchiveRecord Record = Slot.EnterRecord();

	FString TypeName = DistributionTypeToName(Distribution.Type);
	Record << SA_VALUE("Type", TypeName);
	if (bLoading && !DistributionTypeFromName(TypeName, Distribution.Type))
	{
		Distribution.Type = EDistributionType::Constant;
	}

	const EDistributionType Type = Distribution.Type;
	if (TOptional<FStructuredArchiveSlot> Field = Record.TryEnterField("Constant", Type == EDistributionType::Constant))
	{
		Field.GetValue() << Distribution.Constant;
	}
	if (TOptional<FStructuredArchiveSlot> Field = Record.TryEnterField("Min", Type == EDistributionType::Uniform))
	{
		Field.GetValue() << Distribution.Min;
	}
	if (TOptional<FStructuredArchiveSlot> Field = Record.TryEnterField("Max", Type == EDistributionType::Uniform))
	{
		Field.GetValue() << Distribution.Max;
	}
	if (TOptional<FStructuredArchiveSlot> Field = Record.TryEnterField("Curve", Type == EDistributionType::Curve))
	{
		Field.GetValue() << Distribution.Curve;
	}
}
