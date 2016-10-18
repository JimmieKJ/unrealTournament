// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneLevelVisibilitySection.h"


UMovieSceneLevelVisibilitySection::UMovieSceneLevelVisibilitySection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	Visibility = ELevelVisibility::Visible;
}


ELevelVisibility UMovieSceneLevelVisibilitySection::GetVisibility() const
{
	return Visibility;
}


void UMovieSceneLevelVisibilitySection::SetVisibility( ELevelVisibility InVisibility )
{
	Visibility = InVisibility;
}


TArray<FName>* UMovieSceneLevelVisibilitySection::GetLevelNames()
{
	return &LevelNames;
}


TOptional<float> UMovieSceneLevelVisibilitySection::GetKeyTime(FKeyHandle KeyHandle) const
{
	return TOptional<float>();
}


void UMovieSceneLevelVisibilitySection::SetKeyTime(FKeyHandle KeyHandle, float Time)
{
	// do nothing
}
