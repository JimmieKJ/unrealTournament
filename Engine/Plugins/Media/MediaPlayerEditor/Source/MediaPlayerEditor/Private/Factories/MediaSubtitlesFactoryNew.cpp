// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Factories/MediaSubtitlesFactoryNew.h"
#include "MediaSubtitles.h"
#include "AssetTypeCategories.h"


/* UMediaSubtitlesFactoryNew structors
 *****************************************************************************/

UMediaSubtitlesFactoryNew::UMediaSubtitlesFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UMediaSubtitles::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


/* UFactory interface
 *****************************************************************************/

UObject* UMediaSubtitlesFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UMediaSubtitles>(InParent, InClass, InName, Flags);
}


uint32 UMediaSubtitlesFactoryNew::GetMenuCategories() const
{
	return EAssetTypeCategories::Media;
}


bool UMediaSubtitlesFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}
