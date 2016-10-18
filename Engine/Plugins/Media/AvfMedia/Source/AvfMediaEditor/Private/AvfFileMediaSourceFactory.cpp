// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AvfMediaEditorPCH.h"
#include "AvfFileMediaSourceFactory.h"


/* UAvfFileMediaSourceFactory structors
 *****************************************************************************/

UAvfFileMediaSourceFactory::UAvfFileMediaSourceFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Formats.Add(TEXT("m4v;Apple MPEG-4 Video"));
	Formats.Add(TEXT("mov;Apple QuickTime Movie"));
	Formats.Add(TEXT("mp4;MPEG-4 Movie"));

	SupportedClass = UFileMediaSource::StaticClass();
	bEditorImport = true;
}


/* UFactory overrides
 *****************************************************************************/

bool UAvfFileMediaSourceFactory::FactoryCanImport(const FString& Filename)
{
	return true;
}


UObject* UAvfFileMediaSourceFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	UFileMediaSource* MediaSource = NewObject<UFileMediaSource>(InParent, InClass, InName, Flags);
	MediaSource->FilePath = CurrentFilename;

	return MediaSource;
}
