// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPrivatePCH.h"
#include "IMediaModule.h"
#include "IMediaPlayerFactory.h"


/* UMediaPlayerFactory structors
 *****************************************************************************/

UMediaPlayerFactory::UMediaPlayerFactory( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	SupportedClass = UMediaPlayer::StaticClass();

	bCreateNew = false;
	bEditorImport = true;

	// register media framework callbacks
	auto MediaModule = FModuleManager::GetModulePtr<IMediaModule>("Media");

	if (MediaModule != nullptr)
	{
		MediaModule->OnFactoryAdded().AddUObject(this, &UMediaPlayerFactory::HandleMediaPlayerFactoryAdded);
		MediaModule->OnFactoryRemoved().AddUObject(this, &UMediaPlayerFactory::HandleMediaPlayerFactoryRemoved);
	}

	ReloadMediaFormats();
}


/* UFactory overrides
 *****************************************************************************/

UObject* UMediaPlayerFactory::FactoryCreateBinary( UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn )
{
	UMediaPlayer* MediaPlayer = NewObject<UMediaPlayer>(InParent, Class, Name, Flags);
	MediaPlayer->OpenUrl(CurrentFilename);

	return MediaPlayer;
}


/* UMediaPlayerFactory implementation
 *****************************************************************************/

void UMediaPlayerFactory::ReloadMediaFormats()
{
	Formats.Reset();

	auto MediaModule = FModuleManager::GetModulePtr<IMediaModule>("Media");

	if (MediaModule != nullptr)
	{
		FMediaFileTypes FileTypes;
		MediaModule->GetSupportedFileTypes(FileTypes);

		for (auto& Format : FileTypes)
		{
			Formats.Add(Format.Key + TEXT(";") + Format.Value.ToString());
		}
	}
}


/* UMediaPlayerFactory callbacks
 *****************************************************************************/

void UMediaPlayerFactory::HandleMediaPlayerFactoryAdded()
{
	ReloadMediaFormats();
}


void UMediaPlayerFactory::HandleMediaPlayerFactoryRemoved()
{
	ReloadMediaFormats();
}
