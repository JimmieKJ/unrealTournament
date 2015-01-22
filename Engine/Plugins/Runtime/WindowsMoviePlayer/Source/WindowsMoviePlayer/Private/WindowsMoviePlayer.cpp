// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WindowsMoviePlayerPrivatePCH.h"

#include "MoviePlayer.h"

#include "AllowWindowsPlatformTypes.h"
#include <mfapi.h>
#include "HideWindowsPlatformTypes.h"


TSharedPtr<FMediaFoundationMovieStreamer> MovieStreamer;

class FWindowsMoviePlayerModule : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		bool bLoadSuccessful = true;
		// now attempt to load the delay loaded DLLs
		if (LoadLibraryW(TEXT("shlwapi.dll")) == NULL)
		{
			UE_LOG(LogWindowsMoviePlayer, Warning, TEXT("Could not load shlwapi.dll") );
			bLoadSuccessful = false;
		}
		if (LoadLibraryW(TEXT("mf.dll")) == NULL)
		{
			UE_LOG(LogWindowsMoviePlayer, Warning, TEXT("Could not load mf.dll"));
			bLoadSuccessful = false;
		}
		if (LoadLibraryW(TEXT("mfplat.dll")) == NULL)
		{
			UE_LOG(LogWindowsMoviePlayer, Warning, TEXT("Could not load mfplat.dll"));
			bLoadSuccessful = false;
		}
		if (LoadLibraryW(TEXT("mfplay.dll")) == NULL)
		{
			UE_LOG(LogWindowsMoviePlayer, Warning, TEXT("Could not load mfplay.dll"));
			bLoadSuccessful = false;
		}

		if( bLoadSuccessful )
		{
			HRESULT Hr = MFStartup(MF_VERSION);
			check(SUCCEEDED(Hr));

			MovieStreamer = MakeShareable(new FMediaFoundationMovieStreamer);
			GetMoviePlayer()->RegisterMovieStreamer(MovieStreamer);
		}
	}

	virtual void ShutdownModule() override
	{
		if( MovieStreamer.IsValid() )
		{
			MovieStreamer.Reset();

			MFShutdown();
		}
	}
};

IMPLEMENT_MODULE( FWindowsMoviePlayerModule, WindowsMoviePlayer )
