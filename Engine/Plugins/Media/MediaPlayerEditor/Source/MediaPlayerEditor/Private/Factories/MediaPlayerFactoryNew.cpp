// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPrivatePCH.h"


/* UMediaPlayerFactoryNew structors
 *****************************************************************************/

UMediaPlayerFactoryNew::UMediaPlayerFactoryNew( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	SupportedClass = UMediaPlayer::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


/* UFactory overrides
 *****************************************************************************/

UObject* UMediaPlayerFactoryNew::FactoryCreateNew( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn )
{
	return NewObject<UMediaPlayer>(InParent, InClass, InName, Flags);
}


bool UMediaPlayerFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}
