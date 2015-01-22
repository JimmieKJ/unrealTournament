// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Scalability.h"
#include "GameUserSettings.generated.h"

/**
 * Stores user settings for a game (for example graphics and sound settings), with the ability to save and load to and from a file.
 */
UCLASS(config=GameUserSettings, configdonotcheckdefaults)
class ENGINE_API UGameUserSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Applies all current user settings to the game and saves to permanent storage (e.g. file), optionally checking for command line overrides. */
	virtual void ApplySettings(bool bCheckForCommandLineOverrides);

	/** Applies all current user settings to the game and saves to permanent storage (e.g. file). */
	DEPRECATED(4.4, "ApplySettings() is deprecated, use ApplySettings(bool) instead.")
	virtual void ApplySettings();
	
	virtual void ApplyNonResolutionSettings();
	void ApplyResolutionSettings(bool bCheckForCommandLineOverrides);
	void ConditionallyOverrideResolutionSettings();

	/** Returns the user setting for game screen resolution, in pixels. */
	FIntPoint GetScreenResolution() const;

	/** Returns the last confirmed user setting for game screen resolution, in pixels. */
	FIntPoint GetLastConfirmedScreenResolution() const;

	/** Sets the user setting for game screen resolution, in pixels. */
	void SetScreenResolution( FIntPoint Resolution );

	/** Returns the user setting for game window fullscreen mode. */
	EWindowMode::Type GetFullscreenMode() const;

	/** Returns the last confirmed user setting for game window fullscreen mode. */
	EWindowMode::Type GetLastConfirmedFullscreenMode() const;

	/** Sets the user setting for the game window fullscreen mode. See UGameUserSettings::FullscreenMode. */
	void SetFullscreenMode( EWindowMode::Type InFullscreenMode );

	/** Sets the user setting for vsync. See UGameUserSettings::bUseVSync. */
	void SetVSyncEnabled( bool bEnable );

	/** Returns the user setting for vsync. */
	bool IsVSyncEnabled() const;

	/** Checks if the Screen Resolution user setting is different from current */
	bool IsScreenResolutionDirty() const;

	/** Checks if the FullscreenMode user setting is different from current */
	bool IsFullscreenModeDirty() const;

	/** Checks if the vsync user setting is different from current system setting */
	bool IsVSyncDirty() const;

	/** Mark current video mode settings (fullscreenmode/resolution) as being confirmed by the user */
	void ConfirmVideoMode();

	/** Revert video mode (fullscreenmode/resolution) back to the last user confirmed values */
	void RevertVideoMode();

	/** Set scalability settings to sensible fallback values, for use when the benchmark fails or potentially causes a crash */
	void SetBenchmarkFallbackValues();

	/** Checks if any user settings is different from current */
	virtual bool IsDirty() const;

	/** Validates and resets bad user settings to default. Deletes stale user settings file if necessary. */
	virtual void ValidateSettings();

	/** Loads the user settings from persistent storage */
	virtual void LoadSettings( bool bForceReload=false );

	/** Save the user settings to persistent storage */
	virtual void SaveSettings();

	/** This function resets all settings to the current system settings */
	virtual void ResetToCurrentSettings();

	virtual void SetWindowPosition(int32 WindowPosX, int32 WindowPosY);

	virtual FIntPoint GetWindowPosition();
	
	virtual void SetToDefaults();

	/** Loads the resolution settings before is object is available */
	static void PreloadResolutionSettings();

	/** @return The default resolution when no resolution is set */
	static FIntPoint GetDefaultResolution();

	/** @return The default window position when no position is set */
	static FIntPoint GetDefaultWindowPosition();

	/** @return The default window mode when no mode is set */
	static EWindowMode::Type GetDefaultWindowMode();

	/** Loads the user .ini settings into GConfig */
	static void LoadConfigIni( bool bForceReload=false );

	/** Request a change to the specified resolution and window mode. Optionally apply cmd line overrides. */
	static void RequestResolutionChange(int32 InResolutionX, int32 InResolutionY, EWindowMode::Type InWindowMode, bool bInDoOverrides = true);

	/** Whether to use VSync or not. (public to allow UI to connect to it) */
	UPROPERTY(config)
	bool bUseVSync;

	// cached for the UI, current state if stored in console variables
	Scalability::FQualityLevels ScalabilityQuality;

protected:
	/** Game screen resolution width, in pixels. */
	UPROPERTY(config)
	uint32 ResolutionSizeX;

	/** Game screen resolution height, in pixels. */
	UPROPERTY(config)
	uint32 ResolutionSizeY;

	/** Game screen resolution width, in pixels. */
	UPROPERTY(config)
	uint32 LastUserConfirmedResolutionSizeX;

	/** Game screen resolution height, in pixels. */
	UPROPERTY(config)
	uint32 LastUserConfirmedResolutionSizeY;

	/** Window PosX */
	UPROPERTY(config)
	int32 WindowPosX;

	/** Window PosY */
	UPROPERTY(config)
	int32 WindowPosY;

	/** 
	 * Whether or not to use the desktop resolution.  
	 * This value only applies if ResolutionX and ResolutionY have not been set yet and only on desktop platforms
	 */
	UPROPERTY(config)
	bool bUseDesktopResolutionForFullscreen;

	/**
	 * Game window fullscreen mode
	 *	0 = Fullscreen
	 *	1 = Windowed fullscreen
	 *	2 = Windowed
	 *	3 = WindowedMirror
	 */
	UPROPERTY(config)
	int32 FullscreenMode;

	/** Last user confirmed fullscreen mode setting. */
	UPROPERTY(config)
	int32 LastConfirmedFullscreenMode;

	/** All settings will be wiped and set to default if the serialized version differs from UE_GAMEUSERSETTINGS_VERSION. */
	UPROPERTY(config)
	uint32 Version;

	/**
	 * Check if the current version of the game user settings is valid. Sub-classes can override this to provide game-specific versioning as necessary.
	 * @return True if the current version is valid, false if it is not
	 */
	virtual bool IsVersionValid();

	/** Update the version of the game user settings to the current version */
	virtual void UpdateVersion();
};
