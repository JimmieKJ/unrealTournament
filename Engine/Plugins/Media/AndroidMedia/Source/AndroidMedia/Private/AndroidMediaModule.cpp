// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AndroidMediaPCH.h"

#include "AndroidMediaPlayer.h"

#define LOCTEXT_NAMESPACE "FAndroidMediaModule"

class FAndroidMediaModule
	: public IModuleInterface
	, public IMediaPlayerFactory
{
public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		if (IsSupported())
		{
			IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");
			if (nullptr != MediaModule)
			{
				SupportedFormats.Add(TEXT("3gpp"), LOCTEXT("Format3gpp", "3GPP Multimedia File"));
				SupportedFormats.Add(TEXT("aac"), LOCTEXT("FormatAac", "MPEG-2 Advanced Audio Coding File"));
				SupportedFormats.Add(TEXT("mp4"), LOCTEXT("FormatMp4", "MPEG-4 Movie"));
				MediaModule->RegisterPlayerFactory(*this);
			}
		}
	}

	virtual void ShutdownModule() override
	{
		if (IsSupported())
		{
			IMediaModule* MediaModule = FModuleManager::GetModulePtr<IMediaModule>("Media");
			if (nullptr != MediaModule)
			{
				MediaModule->UnregisterPlayerFactory(*this);
			}
		}
	}

	// IMediaPlayerFactory interface

	virtual TSharedPtr<IMediaPlayer> CreatePlayer() override
	{
		if (IsSupported())
		{
			return MakeShareable(new FAndroidMediaPlayer());
		}
		else
		{
			return nullptr;
		}
	}

	virtual const FMediaFormats& GetSupportedFormats() const override
	{
		return SupportedFormats;
	}

private:

	// The collection of supported media formats.
	FMediaFormats SupportedFormats;

	bool IsSupported()
	{
		return FAndroidMisc::GetAndroidBuildVersion() >= 14;
	}
};

IMPLEMENT_MODULE(FAndroidMediaModule, AndroidMedia)

#undef LOCTEXT_NAMESPACE
