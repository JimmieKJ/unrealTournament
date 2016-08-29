// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/** An placeholder implementation of the movie player for the editor */
class FNullGameMoviePlayer : public IGameMoviePlayer,
	public TSharedFromThis<FNullGameMoviePlayer>
{
public:
	static TSharedPtr<FNullGameMoviePlayer> Get()
	{
		if (!MoviePlayer.IsValid())
		{
			MoviePlayer = MakeShareable(new FNullGameMoviePlayer);
		}
		return MoviePlayer;
	}

	/** IGameMoviePlayer Interface */
	virtual void RegisterMovieStreamer(TSharedPtr<IMovieStreamer> InMovieStreamer) override {}
	virtual void SetSlateRenderer(TSharedPtr<FSlateRenderer> InSlateRenderer) override {}
	virtual void Initialize() override {}
	virtual void Shutdown() override {}
	virtual void PassLoadingScreenWindowBackToGame() const override {}
	virtual void SetupLoadingScreen(const FLoadingScreenAttributes& InLoadingScreenAttributes) override {}
	virtual bool PlayMovie() override { return false; }
	virtual void StopMovie() override {}
	virtual void WaitForMovieToFinish() override {}
	virtual bool IsLoadingFinished() const override {return true;}
	virtual bool IsMovieCurrentlyPlaying() const override  {return false;}
	virtual bool LoadingScreenIsPrepared() const override {return false;}
	virtual void SetupLoadingScreenFromIni() override {}
	virtual FOnPrepareLoadingScreen& OnPrepareLoadingScreen() override { return OnPrepareLoadingScreenDelegate; }
	virtual FOnMoviePlaybackFinished& OnMoviePlaybackFinished() override { return OnMoviePlaybackFinishedDelegate; }
	virtual void SetSlateOverlayWidget(TSharedPtr<SWidget> NewOverlayWidget) override { }
	virtual bool WillAutoCompleteWhenLoadFinishes() override { return false; }
	virtual FString GetMovieName() override { return TEXT(""); }
	virtual bool IsLastMovieInPlaylist() override { return false; }


private:
	FNullGameMoviePlayer() {}
	
private:
	/** Singleton handle */
	static TSharedPtr<FNullGameMoviePlayer> MoviePlayer;
	
	/** Called before a movie is queued up to play to configure the movie player accordingly. */
	FOnPrepareLoadingScreen OnPrepareLoadingScreenDelegate;

	FOnMoviePlaybackFinished OnMoviePlaybackFinishedDelegate;
};
