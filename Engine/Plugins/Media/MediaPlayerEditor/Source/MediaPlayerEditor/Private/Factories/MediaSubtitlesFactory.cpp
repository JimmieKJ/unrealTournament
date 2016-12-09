// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Factories/MediaSubtitlesFactory.h"
#include "MediaSubtitles.h"
#include "FileMediaSource.h"


/* UMediaSubtitlesFactory structors
 *****************************************************************************/

UMediaSubtitlesFactory::UMediaSubtitlesFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Formats.Add(TEXT("srt;SubRip Subtitles"));

	SupportedClass = UFileMediaSource::StaticClass();
	bEditorImport = true;
}


/* UFactory overrides
 *****************************************************************************/

UObject* UMediaSubtitlesFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	auto Subtitles = NewObject<UMediaSubtitles>(InParent, InClass, InName, Flags);

	return Subtitles;
}
