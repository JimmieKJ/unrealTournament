// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "ShowFlags.h"
#include "ScriptViewportClient.h"
#include "ViewportSplitScreen.h"
#include "DebugDisplayProperty.h"
#include "TitleSafeZone.h"
#include "GameViewportDelegates.h"
#include "SlateBasics.h"

#include "GameViewportClient.generated.h"


class UGameInstance;
class UNetDriver;
class ULocalPlayer;
class FSceneViewport;
class SViewport;
class SWindow;
class SOverlay;
class UCanvas;


/**
 * Stereoscopic rendering passes.  FULL implies stereoscopic rendering isn't enabled for this pass
 */
enum EStereoscopicPass
{
	eSSP_FULL,
	eSSP_LEFT_EYE,
	eSSP_RIGHT_EYE
};


/**
 * A game viewport (FViewport) is a high-level abstract interface for the
 * platform specific rendering, audio, and input subsystems.
 * GameViewportClient is the engine's interface to a game viewport.
 * Exactly one GameViewportClient is created for each instance of the game.  The
 * only case (so far) where you might have a single instance of Engine, but
 * multiple instances of the game (and thus multiple GameViewportClients) is when
 * you have more than one PIE window running.
 *
 * Responsibilities:
 * propagating input events to the global interactions list
 *
 * @see UGameViewportClient
 */
UCLASS(Within=Engine, transient, config=Engine)
class ENGINE_API UGameViewportClient : public UScriptViewportClient, public FExec
{
	GENERATED_UCLASS_BODY()

public:
	virtual ~UGameViewportClient();

	/** The viewport's console.   Might be null on consoles */
	UPROPERTY()
	class UConsole* ViewportConsole;

	/** @todo document */
	UPROPERTY()
	TArray<struct FDebugDisplayProperty> DebugProperties;

	/** border of safe area */
	struct FTitleSafeZoneArea TitleSafeZone;

	/** Array of the screen data needed for all the different splitscreen configurations */
	TArray<struct FSplitscreenData> SplitscreenInfo;

	int32 MaxSplitscreenPlayers;

	/** if true then the title safe border is drawn */
	uint32 bShowTitleSafeZone:1;

	/** If true, this viewport is a play in editor viewport */
	uint32 bIsPlayInEditorViewport:1;

	/** set to disable world rendering */
	uint32 bDisableWorldRendering:1;

protected:
	/**
	 * The splitscreen type that is actually being used; takes into account the number of players and other factors (such as cinematic mode)
	 * that could affect the splitscreen mode that is actually used.
	 */
	TEnumAsByte<ESplitScreenType::Type> ActiveSplitscreenType;

	/* The relative world context for this viewport */
	UPROPERTY()
	UWorld* World;

	UPROPERTY()
	UGameInstance* GameInstance;

	/** If true will suppress the blue transition text messages. */
	bool bSuppressTransitionMessage;

public:

	/** see enum EViewModeIndex */
	int32 ViewModeIndex;

	/** Rotates controller ids among gameplayers, useful for testing splitscreen with only one controller. */
	UFUNCTION(exec)
	virtual void SSSwapControllers();

	/** Exec for toggling the display of the title safe area */
	UFUNCTION(exec)
	virtual void ShowTitleSafeArea();

	/** Sets the player which console commands will be executed in the context of. */
	UFUNCTION(exec)
	virtual void SetConsoleTarget(int32 PlayerIndex);

	/** Returns a relative world context for this viewport.	 */
	virtual UWorld* GetWorld() const override;

	/* Returns the game viewport */
	FSceneViewport* GetGameViewport();

	/* Returns the widget for this viewport */
	TSharedPtr<SViewport> GetGameViewportWidget();

	/* Returns the relevant game instance for this viewport */
	UGameInstance* GetGameInstance() const;

	virtual void Init(struct FWorldContext& WorldContext, UGameInstance* OwningGameInstance);

public:
	// Begin UObject Interface
	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;
	// End UObject Interface

	// FViewportClient interface.
	virtual void RedrawRequested(FViewport* InViewport) override {}
	virtual bool InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent EventType, float AmountDepressed=1.f, bool bGamepad=false) override;
	virtual bool InputAxis(FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples=1, bool bGamepad=false) override;
	virtual bool InputChar(FViewport* Viewport,int32 ControllerId, TCHAR Character) override;
	virtual bool InputTouch(FViewport* Viewport, int32 ControllerId, uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, FDateTime DeviceTimestamp, uint32 TouchpadIndex) override;
	virtual bool InputMotion(FViewport* Viewport, int32 ControllerId, const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration) override;
	virtual EMouseCursor::Type GetCursor(FViewport* Viewport, int32 X, int32 Y ) override;
	virtual TOptional<TSharedRef<SWidget>> MapCursor(FViewport* Viewport, const FCursorReply& CursorReply) override;
	virtual void Precache() override;
	virtual void Draw(FViewport* Viewport,FCanvas* SceneCanvas) override;
	virtual void ProcessScreenShots(FViewport* Viewport) override;
	virtual TOptional<bool> QueryShowFocus(const EFocusCause InFocusCause) const override;
	virtual void LostFocus(FViewport* Viewport) override;
	virtual void ReceivedFocus(FViewport* Viewport) override;
	virtual bool IsFocused(FViewport* Viewport) override;
	virtual void CloseRequested(FViewport* Viewport) override;
	virtual bool RequiresHitProxyStorage() override { return 0; }
	virtual bool IsOrtho() const override;
	virtual void MouseEnter(FViewport* Viewport, int32 x, int32 y) override;
	virtual void MouseLeave(FViewport* Viewport) override;
	virtual void SetIsSimulateInEditorViewport(bool bInIsSimulateInEditorViewport) override;
	// End of FViewportClient interface.

	// Begin FExec interface.
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd,FOutputDevice& Ar) override;
	// End of FExec interface.

	/**
	 * Exec command handlers
	 */
	bool HandleForceFullscreenCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleShowCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleShowLayerCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleViewModeCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleNextViewModeCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	bool HandlePrevViewModeCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
#if WITH_EDITOR
	bool HandleShowMouseCursorCommand( const TCHAR* Cmd, FOutputDevice& Ar );
#endif // WITH_EDITOR
	bool HandlePreCacheCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleToggleFullscreenCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleSetResCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleHighresScreenshotCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleHighresScreenshotUICommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleScreenshotCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleBugScreenshotwithHUDInfoCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleBugScreenshotCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleKillParticlesCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleForceSkelLODCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );
	bool HandleDisplayCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDisplayAllCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDisplayAllLocationCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDisplayAllRotationCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDisplayClearCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleTextureDefragCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleToggleMIPFadeCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandlePauseRenderClockCommand( const TCHAR* Cmd, FOutputDevice& Ar );

	/**
	 * Adds a widget to the Slate viewport's overlay (i.e for in game UI or tools) at the specified Z-order
	 * 
	 * @param  ViewportContent	The widget to add.  Must be valid.
	 * @param  ZOrder  The Z-order index for this widget.  Larger values will cause the widget to appear on top of widgets with lower values.
	 */
	virtual void AddViewportWidgetContent( TSharedRef<class SWidget> ViewportContent, const int32 ZOrder = 0 );

	/**
	 * Removes a previously-added widget from the Slate viewport
	 *
	 * @param	ViewportContent  The widget to remove.  Must be valid.
	 */
	virtual void RemoveViewportWidgetContent( TSharedRef<class SWidget> ViewportContent );


	/**
	 * This function removes all widgets from the viewport overlay
	 */
	void RemoveAllViewportWidgets();

	/**
	 * Cleans up all rooted or referenced objects created or managed by the GameViewportClient.  This method is called
	 * when this GameViewportClient has been disassociated with the game engine (i.e. is no longer the engine's GameViewport).
	 */
	virtual void DetachViewportClient();

	/**
	 * Called every frame to allow the game viewport to update time based state.
	 * @param	DeltaTime	The time since the last call to Tick.
	 */
	virtual void Tick( float DeltaTime );

	/**
	 * Determines whether this viewport client should receive calls to InputAxis() if the game's window is not currently capturing the mouse.
	 * Used by the UI system to easily receive calls to InputAxis while the viewport's mouse capture is disabled.
	 */
	virtual bool RequiresUncapturedAxisInput() const;


	/**
	 * Set this GameViewportClient's viewport and viewport frame to the viewport specified
	 * @param	InViewportFrame	The viewportframe to set
	 */
	virtual void SetViewportFrame( FViewportFrame* InViewportFrame );

	/**
	 * Set this GameViewportClient's viewport to the viewport specified
	 * @param	InViewportFrame	The viewport to set
	 */
	virtual void SetViewport( FViewport* InViewportFrame );

	/** Assigns the viewport overlay widget to use for this viewport client.  Should only be called when first created */
	void SetViewportOverlayWidget( TSharedPtr< SWindow > InWindow, TSharedRef<SOverlay> InViewportOverlayWidget )
	{
		Window = InWindow;
		ViewportOverlayWidget = InViewportOverlayWidget;
	}

	/** Returns access to this viewport's Slate window */
	TSharedPtr< SWindow > GetWindow()
	{
		 return Window.Pin();
	}
	 
	/** 
	 * Sets bDropDetail and other per-frame detail level flags on the current WorldSettings
	 *
	 * @param DeltaSeconds - amount of time passed since last tick
	 * @see UWorld
	 */
	virtual void SetDropDetail(float DeltaSeconds);

	/**
	 * Process Console Command
	 *
	 * @param	Command		command to process
	 */
	virtual FString ConsoleCommand( const FString& Command );

	/**
	 * Retrieve the size of the main viewport.
	 *
	 * @param	out_ViewportSize	[out] will be filled in with the size of the main viewport
	 */
	void GetViewportSize( FVector2D& ViewportSize ) const;

	/** @return Whether or not the main viewport is fullscreen or windowed. */
	bool IsFullScreenViewport() const;

	/** @return mouse position in game viewport coordinates (does not account for splitscreen) */
	DEPRECATED(4.5, "Use GetMousePosition that returns a boolean if mouse is in window instead.")
	FVector2D GetMousePosition() const;

	/** @return mouse position in game viewport coordinates (does not account for splitscreen) */
	bool GetMousePosition(FVector2D& MousePosition) const;

	/**
	 * Determine whether a fullscreen viewport should be used in cases where there are multiple players.
	 *
	 * @return	true to use a fullscreen viewport; false to allow each player to have their own area of the viewport.
	 */
	bool ShouldForceFullscreenViewport() const;

	/**
	 * Initialize the game viewport.
	 * @param OutError - If an error occurs, returns the error description.
	 * @return False if an error occurred, true if the viewport was initialized successfully.
	 */
	virtual ULocalPlayer* SetupInitialLocalPlayer(FString& OutError);

	DEPRECATED(4.4, "CreatePlayer is deprecated UGameInstance::CreateLocalPlayer instead.")
	virtual ULocalPlayer* CreatePlayer(int32 ControllerId, FString& OutError, bool bSpawnActor);

	DEPRECATED(4.4, "RemovePlayer is deprecated UGameInstance::RemoveLocalPlayer instead.")
	virtual bool RemovePlayer(class ULocalPlayer* ExPlayer);

	/** @return Returns the splitscreen type that is currently being used */
	FORCEINLINE ESplitScreenType::Type GetCurrentSplitscreenConfiguration() const
	{
		return ActiveSplitscreenType;
	}

	/**
	 * Sets the value of ActiveSplitscreenConfiguration based on the desired split-screen layout type, current number of players, and any other
	 * factors that might affect the way the screen should be laid out.
	 */
	virtual void UpdateActiveSplitscreenType();

	/** Called before rendering to allow the game viewport to allocate subregions to players. */
	virtual void LayoutPlayers();

	/** Allows game code to disable splitscreen (useful when in menus) */
	void SetDisableSplitscreenOverride( const bool bDisabled );

	/** called before rending subtitles to allow the game viewport to determine the size of the subtitle area
	 * @param Min top left bounds of subtitle region (0 to 1)
	 * @param Max bottom right bounds of subtitle region (0 to 1)
	 */
	virtual void GetSubtitleRegion(FVector2D& MinPos, FVector2D& MaxPos);

	/**
	* Convert a LocalPlayer to it's index in the GamePlayer array
	* @param LPlayer Player to get the index of
	* @returns -1 if the index could not be found.
	*/
	int32 ConvertLocalPlayerToGamePlayerIndex( ULocalPlayer* LPlayer );

	/** Whether the player at LocalPlayerIndex's viewport has a "top of viewport" safezone or not. */
	bool HasTopSafeZone( int32 LocalPlayerIndex );

	/** Whether the player at LocalPlayerIndex's viewport has a "bottom of viewport" safezone or not.*/
	bool HasBottomSafeZone( int32 LocalPlayerIndex );

	/** Whether the player at LocalPlayerIndex's viewport has a "left of viewport" safezone or not. */
	bool HasLeftSafeZone( int32 LocalPlayerIndex );

	/** Whether the player at LocalPlayerIndex's viewport has a "right of viewport" safezone or not. */
	bool HasRightSafeZone( int32 LocalPlayerIndex );

	/**
	* Get the total pixel size of the screen.
	* This is different from the pixel size of the viewport since we could be in splitscreen
	*/
	void GetPixelSizeOfScreen( float& Width, float& Height, UCanvas* Canvas, int32 LocalPlayerIndex );

	/** Calculate the amount of safezone needed for a single side for both vertical and horizontal dimensions*/
	void CalculateSafeZoneValues( float& Horizontal, float& Vertical, UCanvas* Canvas, int32 LocalPlayerIndex, bool bUseMaxPercent );

	/**
	* pixel size of the deadzone for all sides (right/left/top/bottom) based on which local player it is
	* @return true if the safe zone exists
	*/
	bool CalculateDeadZoneForAllSides( ULocalPlayer* LPlayer, UCanvas* Canvas, float& fTopSafeZone, float& fBottomSafeZone, float& fLeftSafeZone, float& fRightSafeZone, bool bUseMaxPercent = false );

	/**  
	 * Draw the safe area using the current TitleSafeZone settings. 
	 * 
	 * @param Canvas	Canvas on which to draw
	 */
	virtual void DrawTitleSafeArea( UCanvas* Canvas );

	/**
	 * Called after rendering the player views and HUDs to render menus, the console, etc.
	 * This is the last rendering call in the render loop
	 *
	 * @param Canvas	The canvas to use for rendering.
	 */
	virtual void PostRender( UCanvas* Canvas );

	/**
	 * Displays the transition screen.
	 *
	 * @param Canvas	The canvas to use for rendering.
	 */
	virtual void DrawTransition( UCanvas* Canvas );

	/** 
	 * Print a centered transition message with a drop shadow. 
	 * 
	 * @param Canvas	The canvas to use for rendering.
	 * @param Message	Transition message
	 */
	virtual void DrawTransitionMessage( UCanvas* Canvas, const FString& Message );

	/**
	 * Notifies all interactions that a new player has been added to the list of active players.
	 *
	 * @param	PlayerIndex		the index [into the GamePlayers array] where the player was inserted
	 * @param	AddedPlayer		the player that was added
	 */
	virtual void NotifyPlayerAdded( int32 PlayerIndex, class ULocalPlayer* AddedPlayer );

	/**
	 * Notifies all interactions that a new player has been added to the list of active players.
	 *
	 * @param	PlayerIndex		the index [into the GamePlayers array] where the player was located
	 * @param	RemovedPlayer	the player that was removed
	 */
	void NotifyPlayerRemoved( int32 PlayerIndex, class ULocalPlayer* RemovedPlayer );

	/**
	 * Notification of server travel error messages, generally network connection related (package verification, client server handshaking, etc) 
	 * generally not expected to handle the failure here, but provide notification to the user
	 *
	 * @param	FailureType	the type of error
	 * @param	ErrorString	additional string detailing the error
	 */
	virtual void PeekTravelFailureMessages(UWorld* InWorld, enum ETravelFailure::Type FailureType, const FString& ErrorString);

	/**
	 * Notification of network error messages
	 * generally not expected to handle the failure here, but provide notification to the user
	 *
	 * @param	FailureType	the type of error
	 * @param	NetDriverName name of the network driver generating the error
	 * @param	ErrorString	additional string detailing the error
	 */
	virtual void PeekNetworkFailureMessages(UWorld *World, UNetDriver *NetDriver, enum ENetworkFailure::Type FailureType, const FString& ErrorString);

	/** Make sure all navigation objects have appropriate path rendering components set.  Called when EngineShowFlags.Navigation is set. */
	virtual void VerifyPathRenderingComponents();

	/** Accessor for delegate called when a png screenshot is captured */
	FOnPNGScreenshotCaptured& OnPNGScreenshotCaptured()
	{
		return PNGScreenshotCapturedDelegate;
	}

	/** Accessor for delegate called when a screenshot is captured */
	static FOnScreenshotCaptured& OnScreenshotCaptured()
	{
		return ScreenshotCapturedDelegate;
	}

	/* Accessor for the delegate called when a viewport is asked to close. */
	FOnCloseRequested& OnCloseRequested()
	{
		return CloseRequestedDelegate;
	}

	/** Return the engine show flags for this viewport */
	virtual FEngineShowFlags* GetEngineShowFlags() 
	{ 
		return &EngineShowFlags; 
	}

public:
	/** The show flags used by the viewport's players. */
	FEngineShowFlags EngineShowFlags;

	/** The platform-specific viewport which this viewport client is attached to. */
	FViewport* Viewport;

	/** The platform-specific viewport frame which this viewport is contained by. */
	FViewportFrame* ViewportFrame;

	/**
	 * Controls suppression of the blue transition text messages 
	 * 
	 * @param bSuppress	Pass true to suppress messages
	 */
	void SetSuppressTransitionMessage( bool bSuppress )
	{
		bSuppressTransitionMessage = bSuppress;
	}

	/**
	 * Get a ptr to the stat unit data for this viewport
	 */
	virtual FStatUnitData* GetStatUnitData() const override
	{
		return StatUnitData;
	}

	/**
	 * Get a ptr to the stat unit data for this viewport
	 */
	virtual FStatHitchesData* GetStatHitchesData() const override
	{
		return StatHitchesData;
	}

	/**
	 * Get a ptr to the enabled stats list
	 */
	virtual const TArray<FString>* GetEnabledStats() const override
	{
		return &EnabledStats;
	}

	/**
	 * Sets all the stats that should be enabled for the viewport
	 *
	 * @param InEnabledStats	Stats to enable
	 */
	virtual void SetEnabledStats(const TArray<FString>& InEnabledStats) override
	{
		EnabledStats = InEnabledStats;
	}

	/**
	 * Check whether a specific stat is enabled for this viewport
	 *
	 * @param	InName	Name of the stat to check
	 */
	virtual bool IsStatEnabled(const TCHAR* InName) const override
	{
		return EnabledStats.Contains(InName);
	}

	/**
	 * Get the sound stat flags enabled for this viewport
	 */
	virtual ESoundShowFlags::Type GetSoundShowFlags() const override
	{ 
		return SoundShowFlags;
	}

	/**
	 * Set the sound stat flags enabled for this viewport
	 */
	virtual void SetSoundShowFlags(const ESoundShowFlags::Type InSoundShowFlags) override
	{
		SoundShowFlags = InSoundShowFlags;
	}

	/**
	 * Set whether to ignore input.
	 */
	void SetIgnoreInput(bool Ignore)
	{
		bIgnoreInput = Ignore;
	}

	/**
	 * Check whether we should ignore input.
	 */
	virtual bool IgnoreInput() override
	{
		return bIgnoreInput;
	}

	/**
	 * Set the mouse capture behavior when the viewport is clicked
	 */
	void SetCaptureMouseOnClick(EMouseCaptureMode::Type Mode)
	{
		MouseCaptureMode = Mode;
	}

	/**
	 * Gets the mouse capture behavior when the viewport is clicked
	 */
	virtual EMouseCaptureMode::Type CaptureMouseOnClick() override
	{
		return MouseCaptureMode;
	}

	/**
	 * Sets whether or not the cursor is hidden when the viewport captures the mouse
	 */
	void SetHideCursorDuringCapture(bool InHideCursorDuringCapture)
	{
		bHideCursorDuringCapture = InHideCursorDuringCapture;
	}

	/**
	 * Gets whether or not the cursor is hidden when the viewport captures the mouse
	 */
	virtual bool HideCursorDuringCapture() override
	{
		return bHideCursorDuringCapture;
	}

	virtual TOptional<EPopupMethod> OnQueryPopupMethod() const override
	{
		return EPopupMethod::UseCurrentWindow;
	}

private:
	/**
	 * Set a specific stat to either enabled or disabled (returns the number of remaining enabled stats)
	 */
	int32 SetStatEnabled(const TCHAR* InName, const bool bEnable, const bool bAll = false)
	{
		if (bEnable)
		{
			check(!bAll);	// Not possible to enable all
			EnabledStats.AddUnique(InName);
		}
		else
		{
			if (bAll)
			{
				EnabledStats.Empty();
			}
			else
			{
				EnabledStats.Remove(InName);
			}
		}
		return EnabledStats.Num();
	}

	/** Process the 'show collision' console command */
	void ToggleShowCollision();

	/** Delegate handler to see if a stat is enabled on this viewport */
	void HandleViewportStatCheckEnabled(const TCHAR* InName, bool& bOutCurrentEnabled, bool& bOutOthersEnabled);

	/** Delegate handler for when stats are enabled in a viewport */
	void HandleViewportStatEnabled(const TCHAR* InName);

	/** Delegate handler for when stats are disabled in a viewport */
	void HandleViewportStatDisabled(const TCHAR* InName);

	/** Delegate handler for when all stats are disabled in a viewport */
	void HandleViewportStatDisableAll(const bool bInAnyViewport);

	/** Delegate handler for when an actor is spawned */
	void ShowCollisionOnSpawnedActors(AActor* Actor);

private:
	/** Slate window associated with this viewport client.  The same window may host more than one viewport client. */
	TWeakPtr<SWindow> Window;

	/** Overlay widget that contains widgets to draw on top of the game viewport */
	TWeakPtr<SOverlay> ViewportOverlayWidget;

	/** Current buffer visualization mode for this game viewport */
	FName CurrentBufferVisualizationMode;

	/** Weak pointer to the highres screenshot dialog if it's open */
	TWeakPtr<SWindow> HighResScreenshotDialog;

	/** Map of Cursor Widgets*/
	TMap<EMouseCursor::Type, TSharedRef<SWidget>> CursorWidgets;

	/* Function that handles bug screen-shot requests w/ or w/o extra HUD info (project-specific) */
	bool RequestBugScreenShot(const TCHAR* Cmd, bool bDisplayHUDInfo);

	/** 
	 * Applies requested changes to display configuration 
	 * @param	Dimensions - Pointer to new dimensions of the display. NULL for no change.
	 * @param	WindowMode - What window mode do we want to st the display to.
	 */
	bool SetDisplayConfiguration( const FIntPoint* Dimensions, EWindowMode::Type WindowMode);

	/** Delegate called at the end of the frame when a screenshot is captured and a .png is requested */
	FOnPNGScreenshotCaptured PNGScreenshotCapturedDelegate;

	/** Delegate called at the end of the frame when a screenshot is captured */
	static FOnScreenshotCaptured ScreenshotCapturedDelegate;

	/** Delegate called when a request to close the viewport is received */
	FOnCloseRequested CloseRequestedDelegate;

	/** Data needed to display perframe stat tracking when STAT UNIT is enabled */
	FStatUnitData* StatUnitData;

	/** Data needed to display perframe stat tracking when STAT HITCHES is enabled */
	FStatHitchesData* StatHitchesData;

	/** A list of all the stat names which are enabled for this viewport (static so they persist between runs) */
	static TArray<FString> EnabledStats;

	/** Those sound stat flags which are enabled on this viewport */
	static ESoundShowFlags::Type SoundShowFlags;

	/** Number of viewports which have enabled 'show collision' */
	static int32 NumViewportsShowingCollision;

	/** Disables splitscreen, useful when game code is in menus, and doesn't want splitscreen on */
	bool bDisableSplitScreenOverride;

	/** Whether or not to ignore input */
	bool bIgnoreInput;

	/** Mouse capture behavior when the viewport is clicked */
	EMouseCaptureMode::Type MouseCaptureMode;

	/** Whether or not the cursor is hidden when the viewport captures the mouse */
	bool bHideCursorDuringCapture;
};



