// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	virtual bool PlayMovie() override { return false;}
	virtual void StopMovie() override {}
	virtual void WaitForMovieToFinish() override {}
	virtual bool IsLoadingFinished() const override {return true;}
	virtual bool IsMovieCurrentlyPlaying() const override  {return false;}
	virtual bool LoadingScreenIsPrepared() const override {return false;}
	virtual void SetupLoadingScreenFromIni() override {}
	virtual FOnMoviePlaybackFinished& OnMoviePlaybackFinished() override { return OnMoviePlaybackFinishedDelegate; }
private:
	FNullGameMoviePlayer() {}
	
private:
	/** Singleton handle */
	static TSharedPtr<FNullGameMoviePlayer> MoviePlayer;
	FOnMoviePlaybackFinished OnMoviePlaybackFinishedDelegate;
};
