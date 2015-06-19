// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AndroidMediaPCH.h"
#include "AndroidMediaPlayer.h"
#include "IMediaModule.h"
#include "IMediaPlayerFactory.h"


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
				SupportedFileTypes.Add(TEXT("3gpp"), LOCTEXT("Format3gpp", "3GPP Multimedia File"));
				SupportedFileTypes.Add(TEXT("aac"), LOCTEXT("FormatAac", "MPEG-2 Advanced Audio Coding File"));
				SupportedFileTypes.Add(TEXT("mp4"), LOCTEXT("FormatMp4", "MPEG-4 Movie"));
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

	virtual const FMediaFileTypes& GetSupportedFileTypes() const override
	{
		return SupportedFileTypes;
	}

	virtual bool SupportsUrl(const FString& Url) const override
	{
		return SupportedFileTypes.Contains(FPaths::GetExtension(Url));
	}

private:

	// The collection of supported media file types.
	FMediaFileTypes SupportedFileTypes;

	bool IsSupported()
	{
		return FAndroidMisc::GetAndroidBuildVersion() >= 14;
	}
};

IMPLEMENT_MODULE(FAndroidMediaModule, AndroidMedia)

#undef LOCTEXT_NAMESPACE
