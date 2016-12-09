// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneEventSection.h"
#include "EngineGlobals.h"
#include "IMovieScenePlayer.h"

#include "Curves/KeyFrameAlgorithms.h"


/* UMovieSceneSection structors
 *****************************************************************************/

UMovieSceneEventSection::UMovieSceneEventSection()
#if WITH_EDITORONLY_DATA
	: CurveInterface(TCurveInterface<FEventPayload, float>(&EventData.KeyTimes, &EventData.KeyValues, &EventData.KeyHandles))
#else
	: CurveInterface(TCurveInterface<FEventPayload, float>(&EventData.KeyTimes, &EventData.KeyValues))
#endif
{
	SetIsInfinite(true);
}

void UMovieSceneEventSection::PostLoad()
{
	for (FNameCurveKey EventKey : Events_DEPRECATED.GetKeys())
	{
		EventData.KeyTimes.Add(EventKey.Time);
		EventData.KeyValues.Add(FEventPayload(EventKey.Value));
	}

	if (Events_DEPRECATED.GetKeys().Num())
	{
		MarkAsChanged();
	}

	Super::PostLoad();
}

/* UMovieSceneSection overrides
 *****************************************************************************/

void UMovieSceneEventSection::DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles)
{
	Super::DilateSection(DilationFactor, Origin, KeyHandles);

	KeyFrameAlgorithms::Scale(CurveInterface.GetValue(), Origin, DilationFactor, KeyHandles);
}


void UMovieSceneEventSection::GetKeyHandles(TSet<FKeyHandle>& KeyHandles, TRange<float> TimeRange) const
{
	for (auto It(CurveInterface->IterateKeys()); It; ++It)
	{
		if (TimeRange.Contains(*It))
		{
			KeyHandles.Add(It.GetKeyHandle());
		}
	}
}


void UMovieSceneEventSection::MoveSection(float DeltaPosition, TSet<FKeyHandle>& KeyHandles)
{
	Super::MoveSection(DeltaPosition, KeyHandles);

	KeyFrameAlgorithms::Translate(CurveInterface.GetValue(), DeltaPosition, KeyHandles);
}


TOptional<float> UMovieSceneEventSection::GetKeyTime( FKeyHandle KeyHandle ) const
{
	return CurveInterface->GetKeyTime(KeyHandle);
}


void UMovieSceneEventSection::SetKeyTime( FKeyHandle KeyHandle, float Time )
{
	CurveInterface->SetKeyTime(KeyHandle, Time);
}
