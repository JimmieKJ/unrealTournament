// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Containers/ArrayView.h"
#include "MovieSceneSegment.generated.h"

/**
 * Evaluation data that specifies information about what to evaluate for a given template
 */
USTRUCT()
struct FSectionEvaluationData
{
	GENERATED_BODY()

	FSectionEvaluationData() : ForcedTime(TNumericLimits<float>::Lowest()) {}
	explicit FSectionEvaluationData(int32 InImplIndex, float InForcedTime = TNumericLimits<float>::Lowest()) : ImplIndex(InImplIndex), ForcedTime(InForcedTime) {}

	friend bool operator==(FSectionEvaluationData A, FSectionEvaluationData B)
	{
		return A.ImplIndex == B.ImplIndex && A.ForcedTime == B.ForcedTime;
	}

	float GetTime(float InActualTime)
	{
		return ForcedTime == TNumericLimits<float>::Lowest() ? InActualTime : ForcedTime;
	}

	/** The implementation index we should evaluation (index into FMovieSceneEvaluationTrack::ChildTemplates) */
	UPROPERTY()
	int32 ImplIndex;

	/** A forced time to evaluate this section at */
	UPROPERTY()
	float ForcedTime;
};

/**
 * Information about a singe segment of an evaluation track
 */
USTRUCT()
struct FMovieSceneSegment
{
	GENERATED_BODY()

	FMovieSceneSegment()
	{}

	FMovieSceneSegment(const TRange<float>& InRange)
		: Range(InRange)
	{}

	FMovieSceneSegment(const TRange<float>& InRange, TArrayView<int32> InApplicationImpls)
		: Range(InRange)
	{
		Impls.Reserve(InApplicationImpls.Num());
		for (int32 Impl : InApplicationImpls)
		{
			Impls.Add(FSectionEvaluationData(Impl));
		}
	}

	/** Custom serializer to accomodate the inline allocator on our array */
	bool Serialize(FArchive& Ar)
	{
		Ar << Range;

		int32 NumStructs = Impls.Num();
		Ar << NumStructs;

		if (Ar.IsLoading())
		{
			for (int32 Index = 0; Index < NumStructs; ++Index)
			{
				FSectionEvaluationData Data;
				FSectionEvaluationData::StaticStruct()->SerializeItem(Ar, &Data, nullptr);
				Impls.Add(Data);
			}
		}
		else if (Ar.IsSaving())
		{
			for (FSectionEvaluationData& Data : Impls)
			{
				FSectionEvaluationData::StaticStruct()->SerializeItem(Ar, &Data, nullptr);
			}
		}
		return true;
	}

	/** The segment's range */
	FFloatRange Range;

	/** Array of implementations that reside at the segment's range */
	TArray<FSectionEvaluationData, TInlineAllocator<4>> Impls;
};

template<> struct TStructOpsTypeTraits<FMovieSceneSegment> : public TStructOpsTypeTraitsBase
{
	enum { WithSerializer = true };
};
