// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournamentFullScreenMovie.h"
#include "GenericApplication.h"
#include "GenericApplicationMessageHandler.h"
#include "SlateExtras.h"

// This module must be loaded "PreLoadingScreen" in the .uproject file, otherwise it will not hook in time!

class FUnrealTournamentFullScreenMovieModule : public IUnrealTournamentFullScreenMovieModule
{
protected:
	FSlateFontInfo LoadingFont;
	int32 count;
public:

	virtual void StartupModule() override
	{		
		count = 0;
#if !UE_SERVER
		FLoadingScreenAttributes LoadingScreen;
		LoadingScreen.bAutoCompleteWhenLoadingCompletes = false;
		LoadingScreen.bMoviesAreSkippable = true;
		LoadingScreen.PlaybackType = EMoviePlaybackType::MT_LoadingLoop;
		LoadingScreen.WidgetLoadingScreen = SNullWidget::NullWidget;

		LoadingScreen.MoviePaths.Empty();
		LoadingScreen.MoviePaths.Add(TEXT("engine_startup"));
		if (GetMoviePlayer().IsValid()) 
		{
			GetMoviePlayer()->SetupLoadingScreen(LoadingScreen);
			GetMoviePlayer()->OnMovieClipFinished().AddRaw(this, &FUnrealTournamentFullScreenMovieModule::OnMovieClipFinished);
		}
#endif
	}


	virtual bool IsGameModule() const override
	{
		return true;
	}

#if !UE_SERVER

	virtual FOnClipFinished& OnClipFinished() override { return OnClipFinishedDelegate; }

	virtual void OnMovieClipFinished(const FString& ClipName)
	{
		OnClipFinished().Broadcast(ClipName);
	}

	virtual void PlayMovie(const FString& MoviePlayList, TSharedPtr<SWidget> SlateOverlayWidget, bool bSkippable, bool bAutoComplete, TEnumAsByte<EMoviePlaybackType> PlaybackType, bool bForce) override
	{
		if ( IsMoviePlayerEnabled() && !MoviePlayList.IsEmpty() && (!GetMoviePlayer()->IsMovieCurrentlyPlaying() || bForce) )
		{
			// If we are focing a move to play, stop any existing movie.		
			if (bForce && GetMoviePlayer()->IsMovieCurrentlyPlaying())
			{
				StopMovie();
			}

			FLoadingScreenAttributes LoadingScreen;
			LoadingScreen.bAutoCompleteWhenLoadingCompletes = bAutoComplete;
			LoadingScreen.bMoviesAreSkippable = bSkippable;
			LoadingScreen.MoviePaths.Empty();

			if ( FParse::Param( FCommandLine::Get(), TEXT( "nomovie" )) )
			{
				LoadingScreen.MoviePaths.Add(TEXT("load_generic_nosound"));
			}
			else
			{
				MoviePlayList.ParseIntoArray(LoadingScreen.MoviePaths, TEXT(";"), true);
			}

			LoadingScreen.PlaybackType = PlaybackType;
			LoadingScreen.WidgetLoadingScreen = SlateOverlayWidget; 

			if (GetMoviePlayer().IsValid())
			{
				GetMoviePlayer()->SetupLoadingScreen(LoadingScreen);
				GetMoviePlayer()->PlayMovie();
			}
		}
	}

	virtual void StopMovie() override
	{
		if (GetMoviePlayer().IsValid()) GetMoviePlayer()->StopMovie();
	}

	virtual void WaitForMovieToFinished() 
	{
		if (GetMoviePlayer().IsValid())
		{
			GetMoviePlayer()->WaitForMovieToFinish();
		}
	}


	virtual bool IsMoviePlaying() override
	{
		return GetMoviePlayer().IsValid() ? GetMoviePlayer()->IsMovieCurrentlyPlaying() : false;
	}

	virtual void SetSlateOverlayWidget(TSharedPtr<SWidget> NewSlateOverlayWidget) override
	{
		if (GetMoviePlayer().IsValid()) GetMoviePlayer()->SetSlateOverlayWidget(NewSlateOverlayWidget);
	}

protected:
	FOnClipFinished OnClipFinishedDelegate;

#endif
};

IMPLEMENT_GAME_MODULE(FUnrealTournamentFullScreenMovieModule, UnrealTournamentFullScreenMovie);
