// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieSceneFactory.h"
#include "MovieScene.h"

#define LOCTEXT_NAMESPACE "MovieSceneFactory"

UMovieSceneFactory::UMovieSceneFactory( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{

	// @todo sequencer: Only allow users to create new MovieScenes if that feature is turned on globally.
	if( FParse::Param( FCommandLine::Get(), TEXT( "Sequencer" ) ) )
	{
		bCreateNew = true;
	}

	bEditAfterNew = true;

	SupportedClass = UMovieScene::StaticClass();
}


UObject* UMovieSceneFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UMovieScene>(InParent, Name, Flags);
}

#undef LOCTEXT_NAMESPACE