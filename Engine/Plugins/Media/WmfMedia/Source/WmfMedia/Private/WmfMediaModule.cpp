// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "WmfMediaPrivate.h"
#include "IWmfMediaModule.h"

#if WMFMEDIA_SUPPORTED_PLATFORM
	#include "WmfMediaPlayer.h"

	#pragma comment(lib, "mf")
	#pragma comment(lib, "mfplat")
	#pragma comment(lib, "mfplay")
	#pragma comment(lib, "mfuuid")
	#pragma comment(lib, "shlwapi")
#endif


DEFINE_LOG_CATEGORY(LogWmfMedia);

#define LOCTEXT_NAMESPACE "FWmfMediaModule"


/**
 * Implements the WmfMedia module.
 */
class FWmfMediaModule
	: public IWmfMediaModule
{
public:

	/** Default constructor. */
	FWmfMediaModule()
		: Initialized(false)
	{ }

public:

	//~ IWmfMediaModule interface

	virtual TSharedPtr<IMediaPlayer> CreatePlayer() override
	{
#if WMFMEDIA_SUPPORTED_PLATFORM
		if (Initialized)
		{
			return MakeShareable(new FWmfMediaPlayer());
		}
#endif

		return nullptr;
	}

public:

	//~ IModuleInterface interface

	virtual void StartupModule() override
	{
#if WMFMEDIA_SUPPORTED_PLATFORM
		// load required libraries
		if (!LoadRequiredLibraries())
		{
			UE_LOG(LogWmfMedia, Log, TEXT("Failed to load required Windows Media Foundation libraries"));

			return;
		}

		// initialize Windows Media Foundation
		HRESULT Result = MFStartup(MF_VERSION);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Log, TEXT("Failed to initialize Windows Media Foundation, Error %i"), Result);

			return;
		}

		Initialized = true;

#endif //WMFMEDIA_SUPPORTED_PLATFORM
	}

	virtual void ShutdownModule() override
	{
#if WMFMEDIA_SUPPORTED_PLATFORM
		if (!Initialized)
		{
			return;
		}

		// shutdown Windows Media Foundation
		MFShutdown();

		Initialized = false;

#endif //WMFMEDIA_SUPPORTED_PLATFORM
	}

protected:

	/**
	 * Loads all required Windows libraries.
	 *
	 * @return true on success, false otherwise.
	 */
	bool LoadRequiredLibraries()
	{
		if (LoadLibraryW(TEXT("shlwapi.dll")) == nullptr)
		{
			UE_LOG(LogWmfMedia, Log, TEXT("Failed to load shlwapi.dll"));

			return false;
		}

		if (LoadLibraryW(TEXT("mf.dll")) == nullptr)
		{
			UE_LOG(LogWmfMedia, Log, TEXT("Failed to load mf.dll"));

			return false;
		}

		if (LoadLibraryW(TEXT("mfplat.dll")) == nullptr)
		{
			UE_LOG(LogWmfMedia, Log, TEXT("Failed to load mfplat.dll"));

			return false;
		}

		if (LoadLibraryW(TEXT("mfplay.dll")) == nullptr)
		{
			UE_LOG(LogWmfMedia, Log, TEXT("Failed to load mfplay.dll"));

			return false;
		}

		return true;
	}

private:

	/** Whether the module has been initialized. */
	bool Initialized;
};


IMPLEMENT_MODULE(FWmfMediaModule, WmfMedia);


#undef LOCTEXT_NAMESPACE
