// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MoviePlayerPrivatePCH.h"
#include "MoviePlayer.h"

#include "Engine.h"
#include "SlateBasics.h"
#include "RenderingCommon.h"
#include "Slate/SlateTextures.h"
#include "Slate/SceneViewport.h"
#include "GlobalShader.h"

#include "SpinLock.h"
#include "MoviePlayerThreading.h"
#include "DefaultGameMoviePlayer.h"
#include "MoviePlayerSettings.h"
#include "ShaderCompiler.h"


DEFINE_LOG_CATEGORY_STATIC(LogMoviePlayer, Log, All);

class SDefaultMovieBorder : public SBorder
{
public:

	SLATE_BEGIN_ARGS(SDefaultMovieBorder)		
		: _OnKeyDown()
	{}

		SLATE_EVENT(FPointerEventHandler, OnMouseButtonDown)
		SLATE_EVENT(FOnKeyDown, OnKeyDown)
		SLATE_DEFAULT_SLOT(FArguments, Content)

	SLATE_END_ARGS()

	/**
	* Construct this widget
	*
	* @param	InArgs	The declaration data for this widget
	*/
	void Construct(const FArguments& InArgs)
	{
		OnKeyDown = InArgs._OnKeyDown;		

		SBorder::Construct(SBorder::FArguments()			
			.BorderImage(FCoreStyle::Get().GetBrush(TEXT("BlackBrush")))
			.OnMouseButtonDown(InArgs._OnMouseButtonDown)
			.Padding(0)[InArgs._Content.Widget]);
		
	}

	/**
	* Set the handler to be invoked when the user presses a key.
	*
	* @param InHandler   Method to execute when the user presses a key
	*/
	void SetOnOnKeyDown(const FOnKeyDown& InHandler)
	{
		OnKeyDown = InHandler;
	}	

protected:
	
	FOnKeyDown OnKeyDown;	
};

TSharedPtr<FDefaultGameMoviePlayer> FDefaultGameMoviePlayer::MoviePlayer;

TSharedPtr<FDefaultGameMoviePlayer> FDefaultGameMoviePlayer::Get()
{
	if (!MoviePlayer.IsValid())
	{
		MoviePlayer = MakeShareable(new FDefaultGameMoviePlayer);
	}
	return MoviePlayer;
}

FDefaultGameMoviePlayer::FDefaultGameMoviePlayer()
	: FTickableObjectRenderThread(false, true)
	, SyncMechanism(NULL)
	, MovieStreamingIsDone(1)
	, LoadingIsDone(1)
	, bUserCalledFinish(false)
	, LoadingScreenAttributes()
	, LastPlayTime(0.0)
	, bInitialized(false)
{
}

FDefaultGameMoviePlayer::~FDefaultGameMoviePlayer()
{
	if ( bInitialized )
	{
		// This should not happen if initialize was called correctly.  This is a fallback to ensure that the movie player rendering tickable gets unregistered on the rendering thread correctly
		Shutdown();
	}
	else
	{
		// Even when uninitialized we must safely unregister the movie player on the render thread
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(UnregisterMoviePlayerTickable, FDefaultGameMoviePlayer*, MoviePlayer, this,
		{
			MoviePlayer->Unregister();
		});
	}

	FlushRenderingCommands();
}

void FDefaultGameMoviePlayer::RegisterMovieStreamer(TSharedPtr<IMovieStreamer> InMovieStreamer)
{
	MovieStreamer = InMovieStreamer;
}

void FDefaultGameMoviePlayer::SetSlateRenderer(TSharedPtr<FSlateRenderer> InSlateRenderer)
{
	RendererPtr = InSlateRenderer;
}

void FDefaultGameMoviePlayer::Initialize()
{
	UE_LOG(LogMoviePlayer, Log, TEXT("Initializing movie player"));

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(RegisterMoviePlayerTickable, FDefaultGameMoviePlayer*, MoviePlayer, this,
	{
		MoviePlayer->Register();
	});

	bInitialized = true;

	// Initialize shaders, because otherwise they might not be guaranteed to exist at this point
	if (!FPlatformProperties::RequiresCookedData())
	{
		TArray<int32> ShaderMapIds;
		ShaderMapIds.Add(GlobalShaderMapId);
		GShaderCompilingManager->FinishCompilation(TEXT("Global"), ShaderMapIds);
	}

	// Add a delegate to start playing movies when we start loading a map
	FCoreUObjectDelegates::PreLoadMap.AddSP( this, &FDefaultGameMoviePlayer::OnPreLoadMap );
	
	// Shutdown the movie player if the app is exiting
	FCoreDelegates::OnPreExit.AddSP( this, &FDefaultGameMoviePlayer::Shutdown );

	FPlatformSplash::Hide();

	TSharedRef<SWindow> GameWindow = UGameEngine::CreateGameWindow();

	TSharedPtr<SViewport> MovieViewport;

	LoadingScreenContents = SNew(SDefaultMovieBorder)	
		.OnKeyDown(this, &FDefaultGameMoviePlayer::OnLoadingScreenKeyDown)
		.OnMouseButtonDown(this, &FDefaultGameMoviePlayer::OnLoadingScreenMouseButtonDown)
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(this, &FDefaultGameMoviePlayer::GetMovieWidth)
				.HeightOverride(this, &FDefaultGameMoviePlayer::GetMovieHeight)
				[
					SAssignNew(MovieViewport, SViewport)
					.EnableGammaCorrection(false)
					.Visibility(this, &FDefaultGameMoviePlayer::GetViewportVisibility)
				]
			]
			+SOverlay::Slot()
			[
				SAssignNew(LoadingScreenWidgetHolder, SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBorder")))
				.Padding(0)
			]
		];

	if (MovieStreamer.IsValid())
	{
		MovieViewport->SetViewportInterface( MovieStreamer->GetViewportInterface().ToSharedRef() );
	}
	
	MovieViewport->SetActive(true);

	// Register the movie viewport so that it can receive user input.
	if (!FPlatformProperties::SupportsWindowedMode())
	{
		FSlateApplication::Get().RegisterGameViewport( MovieViewport.ToSharedRef() );
	}

	LoadingScreenWindowPtr = GameWindow;
}

void FDefaultGameMoviePlayer::Shutdown()
{
	UE_LOG(LogMoviePlayer, Log, TEXT("Shutting down movie player"));

	StopMovie();
	WaitForMovieToFinish();

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(UnregisterMoviePlayerTickable, FDefaultGameMoviePlayer*, MoviePlayer, this,
	{
		MoviePlayer->Unregister();
	});

	bInitialized = false;

	FCoreUObjectDelegates::PreLoadMap.RemoveAll( this );
	FCoreDelegates::OnPreExit.RemoveAll( this );

	LoadingScreenContents.Reset();
	LoadingScreenWidgetHolder.Reset();
	LoadingScreenWindowPtr.Reset();

	MovieStreamer.Reset();

	LoadingScreenAttributes = FLoadingScreenAttributes();

	if( SyncMechanism )
	{
		SyncMechanism->DestroySlateThread();
		FScopeLock SyncMechanismLock(&SyncMechanismCriticalSection);
		delete SyncMechanism;
		SyncMechanism = NULL;
	}
}
void FDefaultGameMoviePlayer::PassLoadingScreenWindowBackToGame() const
{
	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	if (LoadingScreenWindowPtr.IsValid() && GameEngine)
	{
		GameEngine->GameViewportWindow = LoadingScreenWindowPtr;
	}
	else
	{
		UE_LOG(LogMoviePlayer, Warning, TEXT("PassLoadingScreenWindowBackToGame failed.  No Window") );
	}
}

void FDefaultGameMoviePlayer::SetupLoadingScreen(const FLoadingScreenAttributes& InLoadingScreenAttributes)
{
	LoadingScreenAttributes = InLoadingScreenAttributes;
}

bool FDefaultGameMoviePlayer::PlayMovie()
{
	bool bBeganPlaying = false;

	// Allow systems to hook onto the movie player and provide loading screen data on demand 
	// if it has not been setup explicitly by the user.
	if ( !LoadingScreenIsPrepared() )
	{
		OnPrepareLoadingScreenDelegate.Broadcast();
	}

	if (LoadingScreenIsPrepared() && !IsMovieCurrentlyPlaying() && FPlatformMisc::NumberOfCores() > 1)
	{
		check(LoadingScreenAttributes.IsValid());
		bUserCalledFinish = false;
		
		LastPlayTime = FPlatformTime::Seconds();

		bool bIsInitialized = true;
		if (MovieStreamingIsPrepared())
		{
			bIsInitialized = MovieStreamer->Init(LoadingScreenAttributes.MoviePaths, LoadingScreenAttributes.PlaybackType);
		}

		if (bIsInitialized)
		{
			MovieStreamingIsDone.Set(MovieStreamingIsPrepared() ? 0 : 1);
			LoadingIsDone.Set(0);
			
			LoadingScreenWidgetHolder->SetContent(LoadingScreenAttributes.WidgetLoadingScreen.IsValid() ? LoadingScreenAttributes.WidgetLoadingScreen.ToSharedRef() : SNullWidget::NullWidget);
			LoadingScreenWindowPtr.Pin()->SetContent(LoadingScreenContents.ToSharedRef());
		
			{
				FScopeLock SyncMechanismLock(&SyncMechanismCriticalSection);
				SyncMechanism = new FSlateLoadingSynchronizationMechanism();
				SyncMechanism->Initialize();
			}

			bBeganPlaying = true;
		}
	}

	return bBeganPlaying;
}

void FDefaultGameMoviePlayer::StopMovie()
{
	LastPlayTime = 0;
	bUserCalledFinish = true;
	if (LoadingScreenWidgetHolder.IsValid())
	{
		LoadingScreenWidgetHolder->SetContent( SNullWidget::NullWidget );
	}
}

void FDefaultGameMoviePlayer::WaitForMovieToFinish()
{
	const bool bEnforceMinimumTime = LoadingScreenAttributes.MinimumLoadingScreenDisplayTime >= 0.0f;

	if (LoadingScreenIsPrepared() && ( IsMovieCurrentlyPlaying() || !bEnforceMinimumTime ) )
	{
		if (SyncMechanism)
		{
			SyncMechanism->DestroySlateThread();

			{
				FScopeLock SyncMechanismLock(&SyncMechanismCriticalSection);
				delete SyncMechanism;
				SyncMechanism = NULL;
			}
		}

		if( !bEnforceMinimumTime )
		{
			LoadingIsDone.Set(1);
		}
		
		const bool bAutoCompleteWhenLoadingCompletes = LoadingScreenAttributes.bAutoCompleteWhenLoadingCompletes;
		const bool bWaitForManualStop = LoadingScreenAttributes.bWaitForManualStop;

		FSlateApplication& SlateApp = FSlateApplication::Get();
		// Continue to wait until the user calls finish (if enabled) or when loading completes or the minimum enforced time (if any) has been reached.
		while ( (bWaitForManualStop && !bUserCalledFinish) || (!bUserCalledFinish && ((!bEnforceMinimumTime && !IsMovieStreamingFinished() && !bAutoCompleteWhenLoadingCompletes) || (bEnforceMinimumTime && (FPlatformTime::Seconds() - LastPlayTime) < LoadingScreenAttributes.MinimumLoadingScreenDisplayTime))))
		{
			// If we are in a loading loop, and this is the last movie in the playlist.. assume you can break out.
			if (MovieStreamer.IsValid() && LoadingScreenAttributes.PlaybackType == MT_LoadingLoop && MovieStreamer->IsLastMovieInPlaylist())
			{
				break;
			}

			if (FSlateApplication::IsInitialized())
			{
				// Break out of the loop if the main window is closed during the movie.
				if ( !LoadingScreenWindowPtr.IsValid() )
				{
					break;
				}

				FPlatformMisc::PumpMessages(true);

				SlateApp.PollGameDeviceState();
				// Gives widgets a chance to process any accumulated input
				SlateApp.FinishedInputThisFrame();

				float DeltaTime = SlateApp.GetDeltaTime();				

				//pass this rather than doing ::Get() because the sharedptr isn't threadsafe.
				ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
					WaitingTickStreamer,
					FDefaultGameMoviePlayer*, MoviePlayer, this,
					float, DeltaTime, DeltaTime,
					{
						MoviePlayer->TickStreamer(DeltaTime);
					}
				);
				
				SlateApp.Tick();

				// Synchronize the game thread and the render thread so that the render thread doesn't get too far behind.
				SlateApp.GetRenderer()->Sync();

				FlushRenderingCommands();
			}
		}

		LoadingScreenWidgetHolder->SetContent( SNullWidget::NullWidget );

		LoadingIsDone.Set(1);

		MovieStreamingIsDone.Set(1);

		FlushRenderingCommands();

		if( MovieStreamer.IsValid() )
		{
			MovieStreamer->ForceCompletion();
		}

		// Allow the movie streamer to clean up any resources it uses once there are no movies to play.
		if( MovieStreamer.IsValid() )
		{
			MovieStreamer->Cleanup();
		}
	
		// Finally, clear out the loading screen attributes, forcing users to always
		// explicitly set the loading screen they want (rather than have stale loading screens)
		LoadingScreenAttributes = FLoadingScreenAttributes();

		BroadcastMoviePlaybackFinished();
	}
	else
	{	
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		if (GameEngine)
		{
			GameEngine->SwitchGameWindowToUseGameViewport();
		}

	}
}

bool FDefaultGameMoviePlayer::IsLoadingFinished() const
{
	return LoadingIsDone.GetValue() != 0;
}

bool FDefaultGameMoviePlayer::IsMovieCurrentlyPlaying() const
{
	return SyncMechanism != NULL;
}

bool FDefaultGameMoviePlayer::IsMovieStreamingFinished() const
{
	return MovieStreamingIsDone.GetValue() != 0;
}

void FDefaultGameMoviePlayer::Tick( float DeltaTime )
{
	check(IsInRenderingThread());
	if (LoadingScreenWindowPtr.IsValid() && RendererPtr.IsValid() && !IsLoadingFinished())
	{
		FScopeLock SyncMechanismLock(&SyncMechanismCriticalSection);
		if(SyncMechanism)
		{
			if(SyncMechanism->IsSlateDrawPassEnqueued())
			{
				GFrameNumberRenderThread++;
				GRHICommandList.GetImmediateCommandList().BeginFrame();
				TickStreamer(DeltaTime);
				RendererPtr.Pin()->DrawWindows();
				SyncMechanism->ResetSlateDrawPassEnqueued();
				GRHICommandList.GetImmediateCommandList().EndFrame();
				GRHICommandList.GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
			}
		}
	}
}

void FDefaultGameMoviePlayer::TickStreamer(float DeltaTime)
{	
	if (MovieStreamingIsPrepared() && !IsMovieStreamingFinished())
	{
		const bool bMovieIsDone = MovieStreamer->Tick(DeltaTime);		
		if (bMovieIsDone)
		{
			MovieStreamingIsDone.Set(1);
		}
	}
}

TStatId FDefaultGameMoviePlayer::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FDefaultGameMoviePlayer, STATGROUP_Tickables);
}

bool FDefaultGameMoviePlayer::IsTickable() const
{
	return true;
}

bool FDefaultGameMoviePlayer::LoadingScreenIsPrepared() const
{
	return LoadingScreenAttributes.WidgetLoadingScreen.IsValid() || MovieStreamingIsPrepared();
}

void FDefaultGameMoviePlayer::SetupLoadingScreenFromIni()
{
	// We may have already setup a movie from a startup module
	if( !LoadingScreenAttributes.IsValid() )
	{
		// fill out the attributes
		FLoadingScreenAttributes LoadingScreen;
		LoadingScreen.bAutoCompleteWhenLoadingCompletes = !GetDefault<UMoviePlayerSettings>()->bWaitForMoviesToComplete;
		LoadingScreen.bMoviesAreSkippable = GetDefault<UMoviePlayerSettings>()->bMoviesAreSkippable;

		// look in the settings for a list of loading movies
		GetMutableDefault<UMoviePlayerSettings>()->LoadConfig();
		const TArray<FString>& StartupMovies = GetDefault<UMoviePlayerSettings>()->StartupMovies;

		if (StartupMovies.Num() == 0)
		{
			LoadingScreen.MoviePaths.Add(TEXT("Default_Startup"));
		}
		else
		{
			for (const FString& Movie : StartupMovies)
			{
				if (!Movie.IsEmpty())
				{
					LoadingScreen.MoviePaths.Add(Movie);
				}
			}
		}

		// now setup the actual loading screen
		SetupLoadingScreen(LoadingScreen);
	}
}

bool FDefaultGameMoviePlayer::MovieStreamingIsPrepared() const
{
	return MovieStreamer.IsValid() && LoadingScreenAttributes.MoviePaths.Num() > 0;
}

FVector2D FDefaultGameMoviePlayer::GetMovieSize() const
{
	const FVector2D ScreenSize = LoadingScreenWindowPtr.Pin()->GetClientSizeInScreen();
	if (MovieStreamingIsPrepared())
	{
		const float MovieAspectRatio = MovieStreamer->GetAspectRatio();
		const float ScreenAspectRatio = ScreenSize.X / ScreenSize.Y;
		if (MovieAspectRatio < ScreenAspectRatio)
		{
			return FVector2D(ScreenSize.Y * MovieAspectRatio, ScreenSize.Y);
		}
		else
		{
			return FVector2D(ScreenSize.X, ScreenSize.X / MovieAspectRatio);
		}
	}

	// No movie, so simply return the size of the window
	return ScreenSize;
}

FOptionalSize FDefaultGameMoviePlayer::GetMovieWidth() const
{
	return GetMovieSize().X;
}

FOptionalSize FDefaultGameMoviePlayer::GetMovieHeight() const
{
	return GetMovieSize().Y;
}

EVisibility FDefaultGameMoviePlayer::GetSlateBackgroundVisibility() const
{
	return MovieStreamingIsPrepared() && !IsMovieStreamingFinished() ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility FDefaultGameMoviePlayer::GetViewportVisibility() const
{
	return MovieStreamingIsPrepared() && !IsMovieStreamingFinished() ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply FDefaultGameMoviePlayer::OnLoadingScreenMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& PointerEvent)
{
	return OnAnyDown();
}

FReply FDefaultGameMoviePlayer::OnLoadingScreenKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	return OnAnyDown();
}

FReply FDefaultGameMoviePlayer::OnAnyDown()
{
	if (IsLoadingFinished())
	{
		if (LoadingScreenAttributes.bMoviesAreSkippable)
		{
			MovieStreamingIsDone.Set(1);
			if (MovieStreamer.IsValid())
			{
				MovieStreamer->ForceCompletion();
			}
		}

		if (IsMovieStreamingFinished())
		{
			bUserCalledFinish = true;
		}
	}

	return FReply::Handled();
}

void FDefaultGameMoviePlayer::OnPreLoadMap(const FString& LevelName)
{
	FCoreUObjectDelegates::PostLoadMap.RemoveAll(this);

	if( PlayMovie() )
	{
		FCoreUObjectDelegates::PostLoadMap.AddSP(this, &FDefaultGameMoviePlayer::OnPostLoadMap );
	}
}

void FDefaultGameMoviePlayer::OnPostLoadMap()
{
	WaitForMovieToFinish();
}

void FDefaultGameMoviePlayer::SetSlateOverlayWidget(TSharedPtr<SWidget> NewOverlayWidget)
{
	if (MovieStreamer.IsValid() && LoadingScreenWidgetHolder.IsValid())
	{
		LoadingScreenWidgetHolder->SetContent(NewOverlayWidget.ToSharedRef());
	}

}

bool FDefaultGameMoviePlayer::WillAutoCompleteWhenLoadFinishes()
{
	return LoadingScreenAttributes.bAutoCompleteWhenLoadingCompletes || (LoadingScreenAttributes.PlaybackType == MT_LoadingLoop && (MovieStreamer.IsValid() && MovieStreamer->IsLastMovieInPlaylist()));
}


FString FDefaultGameMoviePlayer::GetMovieName()
{
	return MovieStreamer.IsValid() ? MovieStreamer->GetMovieName() : TEXT("");
}

bool FDefaultGameMoviePlayer::IsLastMovieInPlaylist()
{
	return MovieStreamer.IsValid() ? MovieStreamer->IsLastMovieInPlaylist() : false;
}
