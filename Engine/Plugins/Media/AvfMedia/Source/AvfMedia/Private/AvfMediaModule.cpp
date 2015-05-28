// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AvfMediaPrivatePCH.h"
#include "IMediaModule.h"
#include "IMediaPlayerFactory.h"
#include "ModuleInterface.h"
#include "ModuleManager.h"


DEFINE_LOG_CATEGORY(LogAvfMedia);

#define LOCTEXT_NAMESPACE "FAvfMediaModule"


/**
 * Implements the AVFMedia module.
 */
class FAvfMediaModule
	: public IModuleInterface
	, public IMediaPlayerFactory
{
public:

	/** Default constructor. */
	FAvfMediaModule()
		: Initialized(false)
	{ }

public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		// load required libraries
		IMediaModule* MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

		if (MediaModule == nullptr)
		{
			UE_LOG(LogAvfMedia, Warning, TEXT("Failed to load Media module"));

			return;
		}

		// initialize supported media formats
		SupportedFileTypes.Add(TEXT("avi"), LOCTEXT("FormatAvi", "Audio Video Interleave File"));
		SupportedFileTypes.Add(TEXT("m4v"), LOCTEXT("FormatM4v", "Apple MPEG-4 Video"));
		SupportedFileTypes.Add(TEXT("mov"), LOCTEXT("FormatMov", "Apple QuickTime Movie"));
		SupportedFileTypes.Add(TEXT("mp4"), LOCTEXT("FormatMp4", "MPEG-4 Movie"));

		// register factory
		MediaModule->RegisterPlayerFactory(*this);

		Initialized = true;
	}

	virtual void ShutdownModule() override
	{
		if (!Initialized)
		{
			return;
		}

		Initialized = false;

		// unregister video player factory
		IMediaModule* MediaModule = FModuleManager::GetModulePtr<IMediaModule>("Media");

		if (MediaModule != nullptr)
            
		{
			MediaModule->UnregisterPlayerFactory(*this);
		}
	}

public:

	// IMediaPlayerFactory interface

	virtual TSharedPtr<IMediaPlayer> CreatePlayer() override
	{
		if (Initialized)
		{
			return MakeShareable(new FAvfMediaPlayer());
		}

		return nullptr;
	}

	virtual const FMediaFileTypes& GetSupportedFileTypes() const override
	{
		return SupportedFileTypes;
	}

	virtual bool SupportsUrl(const FString& Url) const override
	{
		return SupportedFileTypes.Contains(FPaths::GetExtension(Url));
	}

protected:

	/**
	 * Loads all required Windows libraries.
	 *
	 * @return true on success, false otherwise.
	 */
	bool LoadRequiredLibraries()
    {
		return true;
	}

private:

	/** Whether the module has been initialized. */
	bool Initialized;

	/** The collection of supported media file types. */
	FMediaFileTypes SupportedFileTypes;
};


IMPLEMENT_MODULE(FAvfMediaModule, AvfMedia);


#undef LOCTEXT_NAMESPACE
