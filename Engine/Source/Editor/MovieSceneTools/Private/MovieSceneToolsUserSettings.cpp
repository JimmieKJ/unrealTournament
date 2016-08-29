// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieSceneToolsUserSettings.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"

UMovieSceneUserThumbnailSettings::UMovieSceneUserThumbnailSettings(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	ThumbnailSize = FIntPoint(128, 72);
	bDrawThumbnails = true;
	Quality = EThumbnailQuality::Normal;
}

void UMovieSceneUserThumbnailSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMovieSceneUserThumbnailSettings, Quality))
	{
		BroadcastRedrawThumbnails();
	}
	
	ThumbnailSize.X = FMath::Clamp(ThumbnailSize.X, 1, 1024);
	ThumbnailSize.Y = FMath::Clamp(ThumbnailSize.Y, 1, 1024);

	SaveConfig();
}