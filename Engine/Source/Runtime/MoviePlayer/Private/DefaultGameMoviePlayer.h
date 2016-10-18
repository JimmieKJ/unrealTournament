// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TickableObjectRenderThread.h"


/** An implementation of the movie player/loading screen we will use */
class FDefaultGameMoviePlayer : public FTickableObjectRenderThread, public IGameMoviePlayer,
	public TSharedFromThis<FDefaultGameMoviePlayer>
{
public:
	static TSharedPtr<FDefaultGameMoviePlayer> Get();
	~FDefaultGameMoviePlayer();

	/** IGameMoviePlayer Interface */
	virtual void RegisterMovieStreamer(TSharedPtr<IMovieStreamer> InMovieStreamer) override;
	virtual void SetSlateRenderer(TSharedPtr<FSlateRenderer> InSlateRenderer) override;
	virtual void Initialize() override;
	virtual void Shutdown() override;
	virtual void PassLoadingScreenWindowBackToGame() const override;
	virtual void SetupLoadingScreen(const FLoadingScreenAttributes& LoadingScreenAttributes) override;
	virtual bool PlayMovie() override;
	virtual void StopMovie() override;
	virtual void WaitForMovieToFinish() override;
	virtual bool IsLoadingFinished() const override;
	virtual bool IsMovieCurrentlyPlaying() const override;
	virtual bool LoadingScreenIsPrepared() const override;
	virtual void SetupLoadingScreenFromIni() override;

	virtual FOnPrepareLoadingScreen& OnPrepareLoadingScreen() override { return OnPrepareLoadingScreenDelegate; }
	virtual FOnMoviePlaybackFinished& OnMoviePlaybackFinished() override { return OnMoviePlaybackFinishedDelegate; }

	/** FTickableObjectRenderThread interface */
	virtual void Tick( float DeltaTime ) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	
	/** Callback for clicking on the viewport */
	FReply OnLoadingScreenMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& PointerEvent);
	FReply OnLoadingScreenKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent);

	virtual void SetSlateOverlayWidget(TSharedPtr<SWidget> NewOverlayWidget) override;

	virtual bool WillAutoCompleteWhenLoadFinishes() override;

	virtual FString GetMovieName() override;
	virtual bool IsLastMovieInPlaylist() override;


private:

	/** Ticks the underlying MovieStreamer.  Must be done exactly once before each DrawWindows call. */
	void TickStreamer(float DeltaTime);

	/** True if we have both a registered movie streamer and movies to stream */
	bool MovieStreamingIsPrepared() const;

	/** True if movie streamer has finished streaming all the movies it wanted to */
	bool IsMovieStreamingFinished() const;

	/** Callbacks for movie sizing for the movie viewport */
	FVector2D GetMovieSize() const;
	FOptionalSize GetMovieWidth() const;
	FOptionalSize GetMovieHeight() const;
	EVisibility GetSlateBackgroundVisibility() const;
	EVisibility GetViewportVisibility() const;	
	
	/** Called via a delegate in the engine when maps start to load */
	void OnPreLoadMap(const FString& LevelName);
	
	/** Called via a delegate in the engine when maps finish loading */
	void OnPostLoadMap();
private:
	FDefaultGameMoviePlayer();

	FReply OnAnyDown();
	
	/** The movie streaming system that will be used by us */
	TSharedPtr<IMovieStreamer> MovieStreamer;
	
	/** The window that the loading screen resides in */
	TWeakPtr<class SWindow> LoadingScreenWindowPtr;
	/** Holds a handle to the slate renderer for our threaded drawing */
	TWeakPtr<class FSlateRenderer> RendererPtr;
	/** The widget which includes all contents of the loading screen, widgets and movie player and all */
	TSharedPtr<class SWidget> LoadingScreenContents;
	/** The widget which holds the loading screen widget passed in via the FLoadingScreenAttributes */
	TSharedPtr<class SBorder> LoadingScreenWidgetHolder;

	/** The threading mechanism with which we handle running slate on another thread */
	class FSlateLoadingSynchronizationMechanism* SyncMechanism;

	/** True if all movies have successfully streamed and completed */
	FThreadSafeCounter MovieStreamingIsDone;

	/** True if the game thread has finished loading */
	FThreadSafeCounter LoadingIsDone;

	/** User has called finish (needed if LoadingScreenAttributes.bAutoCompleteWhenLoadingCompletes is off) */
	bool bUserCalledFinish;
	
	/** Attributes of the loading screen we are currently displaying */
	FLoadingScreenAttributes LoadingScreenAttributes;

	/** Called before a movie is queued up to play to configure the movie player accordingly. */
	FOnPrepareLoadingScreen OnPrepareLoadingScreenDelegate;
	
	FOnMoviePlaybackFinished OnMoviePlaybackFinishedDelegate;

	/** The last time a movie was started */
	double LastPlayTime;

	/** True if the movie player has been initialized */
	bool bInitialized;

private:
	/** Singleton handle */
	static TSharedPtr<FDefaultGameMoviePlayer> MoviePlayer;

	/** Critical section to allow the slate loading thread and the render thread to safely utilize the synchronization mechanism for ticking Slate. */
	FCriticalSection SyncMechanismCriticalSection;
};
