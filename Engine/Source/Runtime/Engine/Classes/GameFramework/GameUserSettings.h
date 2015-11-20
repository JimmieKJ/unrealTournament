// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Scalability.h"
#include "GameUserSettings.generated.h"

#if !CPP      //noexport class

/** Supported windowing modes (mirrored from GenericWindow.h) */
UENUM(BlueprintType)
namespace EWindowMode
{
	enum Type
	{
		/** The window is in true fullscreen mode */
		Fullscreen,
		/** The window has no border and takes up the entire area of the screen */
		WindowedFullscreen,
		/** The window has a border and may not take up the entire screen area */
		Windowed,
		/** Pseudo-fullscreen mode for devices like HMDs */
		WindowedMirror
	};
}

#endif

/**
 * Stores user settings for a game (for example graphics and sound settings), with the ability to save and load to and from a file.
 */
UCLASS(config=GameUserSettings, configdonotcheckdefaults)
class ENGINE_API UGameUserSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Applies all current user settings to the game and saves to permanent storage (e.g. file), optionally checking for command line overrides. */
	UFUNCTION(BlueprintCallable, Category=Settings, meta=(bCheckForCommandLineOverrides=true))
	virtual void ApplySettings(bool bCheckForCommandLineOverrides);
	
	UFUNCTION(BlueprintCallable, Category=Settings)
	virtual void ApplyNonResolutionSettings();

	UFUNCTION(BlueprintCallable, Category=Settings)
	void ApplyResolutionSettings(bool bCheckForCommandLineOverrides);

	/** Returns the user setting for game screen resolution, in pixels. */
	UFUNCTION(BlueprintPure, Category=Settings)
	FIntPoint GetScreenResolution() const;

	/** Returns the last confirmed user setting for game screen resolution, in pixels. */
	UFUNCTION(BlueprintPure, Category=Settings)
	FIntPoint GetLastConfirmedScreenResolution() const;

	/** Sets the user setting for game screen resolution, in pixels. */
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetScreenResolution(FIntPoint Resolution);

	/** Returns the user setting for game window fullscreen mode. */
	UFUNCTION(BlueprintPure, Category=Settings)
	EWindowMode::Type GetFullscreenMode() const;

	/** Returns the last confirmed user setting for game window fullscreen mode. */
	UFUNCTION(BlueprintPure, Category=Settings)
	EWindowMode::Type GetLastConfirmedFullscreenMode() const;

	/** Sets the user setting for the game window fullscreen mode. See UGameUserSettings::FullscreenMode. */
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetFullscreenMode(EWindowMode::Type InFullscreenMode);

	/** Sets the user setting for vsync. See UGameUserSettings::bUseVSync. */
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetVSyncEnabled(bool bEnable);

	/** Returns the user setting for vsync. */
	UFUNCTION(BlueprintPure, Category=Settings)
	bool IsVSyncEnabled() const;

	/** Checks if the Screen Resolution user setting is different from current */
	UFUNCTION(BlueprintPure, Category=Settings)
	bool IsScreenResolutionDirty() const;

	/** Checks if the FullscreenMode user setting is different from current */
	UFUNCTION(BlueprintPure, Category=Settings)
	bool IsFullscreenModeDirty() const;

	/** Checks if the vsync user setting is different from current system setting */
	UFUNCTION(BlueprintPure, Category=Settings)
	bool IsVSyncDirty() const;

	/** Mark current video mode settings (fullscreenmode/resolution) as being confirmed by the user */
	UFUNCTION(BlueprintCallable, Category=Settings)
	void ConfirmVideoMode();

	/** Revert video mode (fullscreenmode/resolution) back to the last user confirmed values */
	UFUNCTION(BlueprintCallable, Category=Settings)
	void RevertVideoMode();

	/** Set scalability settings to sensible fallback values, for use when the benchmark fails or potentially causes a crash */
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetBenchmarkFallbackValues();

	/** Sets the user's audio quality level setting */
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetAudioQualityLevel(int32 QualityLevel);

	/** Returns the user's audio quality level setting */
	UFUNCTION(BlueprintPure, Category=Settings)
	int32 GetAudioQualityLevel() const { return AudioQualityLevel; }

	/** Sets the user's frame rate limit (0 will disable frame rate limiting) */
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetFrameRateLimit(float NewLimit);

	/** Gets the user's frame rate limit (0 indiciates the frame rate limit is disabled) */
	UFUNCTION(BlueprintPure, Category=Settings)
	float GetFrameRateLimit() const;

	// Changes all scalability settings at once based on a single overall quality level
	// @param Value 0:low, 1:medium, 2:high, 3:epic
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetOverallScalabilityLevel(int32 Value);

	// Returns the overall scalability level (can return -1 if the settings are custom)
	UFUNCTION(BlueprintCallable, Category=Settings)
	int32 GetOverallScalabilityLevel() const;

	// Returns the current resolution scale and the range
	UFUNCTION(BlueprintCallable, Category=Settings)
	void GetResolutionScaleInformation(float& CurrentScaleNormalized, int32& CurrentScaleValue, int32& MinScaleValue, int32& MaxScaleValue) const;

	// Sets the current resolution scale
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetResolutionScaleValue(int32 NewScaleValue);

	// Sets the current resolution scale as a normalized 0..1 value between MinScaleValue and MaxScaleValue
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetResolutionScaleNormalized(float NewScaleNormalized);

	// Sets the view distance quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetViewDistanceQuality(int32 Value);

	// Returns the view distance quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	int32 GetViewDistanceQuality() const;

	// Sets the shadow quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetShadowQuality(int32 Value);

	// Returns the shadow quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	int32 GetShadowQuality() const;

	// Sets the anti-aliasing quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetAntiAliasingQuality(int32 Value);

	// Returns the anti-aliasing quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	int32 GetAntiAliasingQuality() const;

	// Sets the texture quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetTextureQuality(int32 Value);

	// Returns the texture quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	int32 GetTextureQuality() const;

	// Sets the visual effects quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetVisualEffectQuality(int32 Value);

	// Returns the visual effects quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	int32 GetVisualEffectQuality() const;

	// Sets the post-processing quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SetPostProcessingQuality(int32 Value);

	// Returns the post-processing quality (0..3, higher is better)
	UFUNCTION(BlueprintCallable, Category=Settings)
	int32 GetPostProcessingQuality() const;

	/** Checks if any user settings is different from current */
	UFUNCTION(BlueprintPure, Category=Settings)
	virtual bool IsDirty() const;

	/** Validates and resets bad user settings to default. Deletes stale user settings file if necessary. */
	UFUNCTION(BlueprintCallable, Category=Settings)
	virtual void ValidateSettings();

	/** Loads the user settings from persistent storage */
	UFUNCTION(BlueprintCallable, Category=Settings)
	virtual void LoadSettings(bool bForceReload = false);

	/** Save the user settings to persistent storage (automatically happens as part of ApplySettings) */
	UFUNCTION(BlueprintCallable, Category=Settings)
	virtual void SaveSettings();

	/** This function resets all settings to the current system settings */
	UFUNCTION(BlueprintCallable, Category=Settings)
	virtual void ResetToCurrentSettings();

	virtual void SetWindowPosition(int32 WindowPosX, int32 WindowPosY);

	virtual FIntPoint GetWindowPosition();
	
	UFUNCTION(BlueprintCallable, Category=Settings)
	virtual void SetToDefaults();

	/** Loads the resolution settings before is object is available */
	static void PreloadResolutionSettings();

	/** @return The default resolution when no resolution is set */
	UFUNCTION(BlueprintCallable, Category=Settings)
	static FIntPoint GetDefaultResolution();

	/** @return The default window position when no position is set */
	UFUNCTION(BlueprintCallable, Category=Settings)
	static FIntPoint GetDefaultWindowPosition();

	/** @return The default window mode when no mode is set */
	UFUNCTION(BlueprintCallable, Category=Settings)
	static EWindowMode::Type GetDefaultWindowMode();

	/** Loads the user .ini settings into GConfig */
	static void LoadConfigIni(bool bForceReload = false);

	/** Request a change to the specified resolution and window mode. Optionally apply cmd line overrides. */
	static void RequestResolutionChange(int32 InResolutionX, int32 InResolutionY, EWindowMode::Type InWindowMode, bool bInDoOverrides = true);

	/** Returns the game local machine settings (resolution, windowing mode, scalability settings, etc...) */
	UFUNCTION(BlueprintCallable, Category=Settings)
	static UGameUserSettings* GetGameUserSettings();

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

	UPROPERTY(config)
	int32 AudioQualityLevel;

	/** Frame rate cap */
	UPROPERTY(config)
	float FrameRateLimit;

	/**
	 * Check if the current version of the game user settings is valid. Sub-classes can override this to provide game-specific versioning as necessary.
	 * @return True if the current version is valid, false if it is not
	 */
	virtual bool IsVersionValid();

	/** Update the version of the game user settings to the current version */
	virtual void UpdateVersion();
};
