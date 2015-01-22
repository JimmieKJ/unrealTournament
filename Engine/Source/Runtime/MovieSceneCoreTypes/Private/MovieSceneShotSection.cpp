// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneShotSection.h"

UMovieSceneShotSection::UMovieSceneShotSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
}

void UMovieSceneShotSection::SetCameraGuid(const FGuid& InGuid)
{
	CameraGuid = InGuid;
}

FGuid UMovieSceneShotSection::GetCameraGuid() const
{
	return CameraGuid;
}

void UMovieSceneShotSection::SetTitle(const FText& InTitle)
{
	Title = InTitle;
}

FText UMovieSceneShotSection::GetTitle() const
{
	return Title;
}
