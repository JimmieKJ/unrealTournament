// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AndroidMoviePlayerPrivatePCH.h"

#include "MoviePlayer.h"

TSharedPtr<FAndroidMediaPlayerStreamer> AndroidMovieStreamer;

class FAndroidMoviePlayerModule : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		if (IsSupported())
		{
			FAndroidMediaPlayerStreamer* Streamer = new FAndroidMediaPlayerStreamer;
			AndroidMovieStreamer = MakeShareable(Streamer);
			GetMoviePlayer()->RegisterMovieStreamer(AndroidMovieStreamer);
		}
	}

	virtual void ShutdownModule() override
	{
		if (IsSupported())
		{
			AndroidMovieStreamer->Cleanup();
		}
	}

private:

	bool IsSupported()
	{
		return FAndroidMisc::GetAndroidBuildVersion() >= 14;
	}
};

IMPLEMENT_MODULE( FAndroidMoviePlayerModule, AndroidMoviePlayer )
