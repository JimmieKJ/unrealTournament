// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneColorSection.h"
#include "MovieSceneColorTrack.h"


/* FMovieSceneColorKeyStruct interface
 *****************************************************************************/

void FMovieSceneColorKeyStruct::PropagateChanges(const FPropertyChangedEvent& ChangeEvent)
{
	for (int32 Index = 0; Index <= 3; ++Index)
	{
		Keys[Index]->Value = Color.Component(Index);
	}
}


/* UMovieSceneColorSection structors
 *****************************************************************************/

UMovieSceneColorSection::UMovieSceneColorSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{ }


/* UMovieSceneColorSection interface
 *****************************************************************************/

FLinearColor UMovieSceneColorSection::Eval(float Position, const FLinearColor& DefaultColor) const
{
	return FLinearColor(
		RedCurve.Eval(Position, DefaultColor.R),
		GreenCurve.Eval(Position, DefaultColor.G),
		BlueCurve.Eval(Position, DefaultColor.B),
		AlphaCurve.Eval(Position, DefaultColor.A));
}


/* UMovieSceneSection interface
 *****************************************************************************/

void UMovieSceneColorSection::MoveSection(float DeltaTime, TSet<FKeyHandle>& KeyHandles)
{
	Super::MoveSection(DeltaTime, KeyHandles);

	// Move all the curves in this section
	RedCurve.ShiftCurve(DeltaTime, KeyHandles);
	GreenCurve.ShiftCurve(DeltaTime, KeyHandles);
	BlueCurve.ShiftCurve(DeltaTime, KeyHandles);
	AlphaCurve.ShiftCurve(DeltaTime, KeyHandles);
}


void UMovieSceneColorSection::DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles)
{
	Super::DilateSection(DilationFactor, Origin, KeyHandles);

	RedCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
	GreenCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
	BlueCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
	AlphaCurve.ScaleCurve(Origin, DilationFactor, KeyHandles);
}


void UMovieSceneColorSection::GetKeyHandles(TSet<FKeyHandle>& KeyHandles) const
{
	for (auto It(RedCurve.GetKeyHandleIterator()); It; ++It)
	{
		float Time = RedCurve.GetKeyTime(It.Key());
		if (IsTimeWithinSection(Time))
		{
			KeyHandles.Add(It.Key());
		}
	}

	for (auto It(GreenCurve.GetKeyHandleIterator()); It; ++It)
	{
		float Time = GreenCurve.GetKeyTime(It.Key());
		if (IsTimeWithinSection(Time))
		{
			KeyHandles.Add(It.Key());
		}
	}

	for (auto It(BlueCurve.GetKeyHandleIterator()); It; ++It)
	{
		float Time = BlueCurve.GetKeyTime(It.Key());
		if (IsTimeWithinSection(Time))
		{
			KeyHandles.Add(It.Key());
		}
	}

	for (auto It(AlphaCurve.GetKeyHandleIterator()); It; ++It)
	{
		float Time = AlphaCurve.GetKeyTime(It.Key());
		if (IsTimeWithinSection(Time))
			KeyHandles.Add(It.Key());
	}
}


TSharedPtr<FStructOnScope> UMovieSceneColorSection::GetKeyStruct(const TArray<FKeyHandle>& KeyHandles)
{
	TSharedRef<FStructOnScope> KeyStruct = MakeShareable(new FStructOnScope(FMovieSceneColorKeyStruct::StaticStruct()));
	auto Struct = (FMovieSceneColorKeyStruct*)KeyStruct->GetStructMemory();
	{
		Struct->Keys[0] = RedCurve.GetFirstMatchingKey(KeyHandles);
		Struct->Keys[1] = GreenCurve.GetFirstMatchingKey(KeyHandles);
		Struct->Keys[2] = BlueCurve.GetFirstMatchingKey(KeyHandles);
		Struct->Keys[3] = AlphaCurve.GetFirstMatchingKey(KeyHandles);

		for (int32 Index = 0; Index <= 3; ++Index)
		{
			check(Struct->Keys[Index] != nullptr);
			Struct->Color.Component(Index) = Struct->Keys[Index]->Value;
		}
	}

	return KeyStruct;
}


/* IKeyframeSection interface
 *****************************************************************************/

template<typename CurveType>
CurveType* GetCurveForChannel(EKeyColorChannel Channel, CurveType* RedCurve, CurveType* GreenCurve, CurveType* BlueCurve, CurveType* AlphaCurve)
{
	switch (Channel)
	{
	case EKeyColorChannel::Red:
		return RedCurve;
	case EKeyColorChannel::Green:
		return GreenCurve;
	case EKeyColorChannel::Blue:
		return BlueCurve;
	case EKeyColorChannel::Alpha:
		return AlphaCurve;
	default:
		checkf(false, TEXT("Invalid key color channel"));
		return nullptr;
	}
}


void UMovieSceneColorSection::AddKey(float Time, const FColorKey& Key, EMovieSceneKeyInterpolation KeyInterpolation)
{
	FRichCurve* ChannelCurve = GetCurveForChannel(Key.Channel, &RedCurve, &GreenCurve, &BlueCurve, &AlphaCurve);
	AddKeyToCurve(*ChannelCurve, Time, Key.ChannelValue, KeyInterpolation);
}


bool UMovieSceneColorSection::NewKeyIsNewData(float Time, const FColorKey& Key) const
{
	const FRichCurve* ChannelCurve = GetCurveForChannel(Key.Channel, &RedCurve, &GreenCurve, &BlueCurve, &AlphaCurve);
	return FMath::IsNearlyEqual(ChannelCurve->Eval(Time), Key.ChannelValue) == false;
}


bool UMovieSceneColorSection::HasKeys(const FColorKey& Key) const
{
	const FRichCurve* ChannelCurve = GetCurveForChannel(Key.Channel, &RedCurve, &GreenCurve, &BlueCurve, &AlphaCurve);
	return ChannelCurve->GetNumKeys() != 0;
}


void UMovieSceneColorSection::SetDefault(const FColorKey& Key)
{
	FRichCurve* ChannelCurve = GetCurveForChannel(Key.Channel, &RedCurve, &GreenCurve, &BlueCurve, &AlphaCurve);
	return SetCurveDefault(*ChannelCurve, Key.ChannelValue);
}
