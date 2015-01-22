// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NewsFeedPrivatePCH.h"


/* UNewsFeedSettings structors
 *****************************************************************************/

UNewsFeedSettings::UNewsFeedSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, MaxItemsToShow(10)
	, ShowOnlyUnreadItems(true)
{ }


/* UObject overrides
 *****************************************************************************/

void UNewsFeedSettings::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent )
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	SettingChangedEvent.Broadcast(Name);
}
