// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneShotSection.h"


/* UMovieSceneShotSection interface
 *****************************************************************************/

void UMovieSceneShotSection::SetShotNameAndNumber(const FText& InDisplayName, int32 InShotNumber)
{
	if (TryModify())
	{
		DisplayName = InDisplayName;
		ShotNumber = InShotNumber;
	}
}
