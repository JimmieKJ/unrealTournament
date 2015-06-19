// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPrivatePCH.h"


/* UMediaTextureFactoryNew structors
 *****************************************************************************/

UMediaTextureFactoryNew::UMediaTextureFactoryNew( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	SupportedClass = UMediaTexture::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}


/* UFactory overrides
 *****************************************************************************/

UObject* UMediaTextureFactoryNew::FactoryCreateNew( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn )
{
	UMediaTexture* MediaTexture = NewObject<UMediaTexture>(InParent, InClass, InName, Flags);

	if ((MediaTexture != nullptr) && (InitialMediaPlayer != nullptr))
	{
		MediaTexture->SetMediaPlayer(InitialMediaPlayer);
	}

	return MediaTexture;
}


bool UMediaTextureFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}
