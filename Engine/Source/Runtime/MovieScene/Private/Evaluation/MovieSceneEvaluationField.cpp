// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneEvaluationField.h"

int32 FMovieSceneEvaluationField::GetSegmentFromTime(float Time) const
{
	// Linear search
	// @todo: accelerated search based on the last evaluated index?
	for (int32 Index = 0; Index < Ranges.Num(); ++Index)
	{
		if (Ranges[Index].Contains(Time))
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

TRange<int32> FMovieSceneEvaluationField::OverlapRange(TRange<float> Range) const
{
	int32 StartIndex = 0, Num = 0;
	for (int32 Index = 0; Index < Ranges.Num(); ++Index)
	{
		if (Ranges[Index].Overlaps(Range))
		{
			if (Num == 0)
			{
				StartIndex = Index;
			}
			++Num;
		}
		else if (Num != 0)
		{
			break;
		}
	}

	return Num != 0 ? TRange<int32>(StartIndex, StartIndex + Num) : TRange<int32>::Empty();
}
