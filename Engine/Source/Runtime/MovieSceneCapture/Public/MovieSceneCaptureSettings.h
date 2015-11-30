// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "MovieSceneCaptureSettings.generated.h"

UENUM()
enum class EMovieCaptureType : uint8
{
	AVI		UMETA(DisplayName="AVI Movie"),
	BMP		UMETA(DisplayName="BMP Image Sequence"),
	PNG		UMETA(DisplayName="PNG Image Sequence"),
	JPEG	UMETA(DisplayName="JPEG Image Sequence")
};

/** Structure representing a capture resolution */
USTRUCT()
struct MOVIESCENECAPTURE_API FCaptureResolution
{
	FCaptureResolution(uint32 InX = 0, uint32 InY = 0) : ResX(InX), ResY(InY) {}
	
	GENERATED_BODY()

	UPROPERTY(config, EditAnywhere, Category=Resolution, meta=(ClampMin = 16, ClampMax=7680))
	uint32 ResX;

	UPROPERTY(config, EditAnywhere, Category=Resolution, meta=(ClampMin = 16, ClampMax=7680))
	uint32 ResY;
};

/** Common movie-scene capture settings */
USTRUCT()
struct MOVIESCENECAPTURE_API FMovieSceneCaptureSettings
{
	FMovieSceneCaptureSettings();

	GENERATED_BODY()

	/** The directory to output the captured file(s) in */
	UPROPERTY(config, EditAnywhere, Category=General, meta=(RelativePath))
	FDirectoryPath OutputDirectory;

	/** Whether to save temporary copies of all of the levels before capturing the movie.  This allows you to record movies of temporary work, or work that isn't yet saved, but it will take much longer for capturing to begin. */
	UPROPERTY(config, EditAnywhere, Category=General, AdvancedDisplay)
	bool bCreateTemporaryCopiesOfLevels;

	/** Optional game mode to override the map's default game mode with.  This can be useful if the game's normal mode displays UI elements or loading screens that you don't want captured. */
	UPROPERTY(config, EditAnywhere, Category=General, AdvancedDisplay)
	TSubclassOf<class AGameMode> GameModeOverride;

	/** The format to use for the resulting filename. Extension will be added automatically. Any tokens of the form {token} will be replaced with the corresponding value:
	 * {fps}		- The captured framerate
	 * {frame}		- The current frame number (only relevant for image sequences)
	 * {width}		- The width of the captured frames
	 * {height}		- The height of the captured frames
	 * {world}		- The name of the current world
	 * {quality}	- The image compression quality setting
	 */
	UPROPERTY(config, EditAnywhere, Category=General, DisplayName="Filename Format")
	FString OutputFormat;

	/** Whether to overwrite existing files or not */
	UPROPERTY(config, EditAnywhere, Category=General, AdvancedDisplay )
	bool bOverwriteExisting;

	/** True if frame numbers in the output files should be relative to zero, rather than the actual frame numbers in the originating animation content */
	UPROPERTY(config, EditAnywhere, Category=General, AdvancedDisplay)
	bool bUseRelativeFrameNumbers;

	/** The frame rate at which to capture */
	UPROPERTY(config, EditAnywhere, Category=CaptureSettings)
	int32 FrameRate;

	/** The resolution at which to capture */
	UPROPERTY(config, EditAnywhere, Category=CaptureSettings, meta=(ShowOnlyInnerProperties))
	FCaptureResolution Resolution;

	/** The type of capture to perform */
	UPROPERTY(config, EditAnywhere, Category=CaptureSettings)
	EMovieCaptureType CaptureType;

	/** Whether compression is enabled on the resulting file(s) */
	UPROPERTY(config, EditAnywhere, Category=CaptureSettings, AdvancedDisplay )
	bool bUseCompression;

	/** For output formats that support compression, this specifies the quality level of the compressed image.  0.0 results in the most compression, while 1.0 gives you the highest image quality. */
	UPROPERTY(config, EditAnywhere, Category=CaptureSettings, AdvancedDisplay, meta=(ClampMin=0, ClampMax=1), meta=(EditCondition="bUseCompression"))
	float CompressionQuality;

	/** (Experimental) - An optional codec to use for video encoding */
	UPROPERTY(config, EditAnywhere, Category=CaptureSettings, AdvancedDisplay)
	FString Codec;
	
	/** Whether to texture streaming should be enabled while capturing.  Turning off texture streaming may cause much more memory to be used, but also reduces the chance of blurry textures in your captured video. */
	UPROPERTY(config, EditAnywhere, Category=CaptureSettings, AdvancedDisplay)
	bool bEnableTextureStreaming;

	/** Whether to enable cinematic mode whilst capturing */
	UPROPERTY(config, EditAnywhere, Category=Cinematic)
	bool bCinematicMode;

	/** Whether to allow player movement whilst capturing */
	UPROPERTY(config, EditAnywhere, Category=Cinematic, AdvancedDisplay, meta=(EditCondition="bCinematicMode"))
	bool bAllowMovement;

	/** Whether to allow player rotation whilst capturing */
	UPROPERTY(config, EditAnywhere, Category=Cinematic, AdvancedDisplay, meta=(EditCondition="bCinematicMode"))
	bool bAllowTurning;

	/** Whether to show the local player whilst capturing */
	UPROPERTY(config, EditAnywhere, Category=Cinematic, AdvancedDisplay, meta=(EditCondition="bCinematicMode"))
	bool bShowPlayer;

	/** Whether to show the in-game HUD whilst capturing */
	UPROPERTY(config, EditAnywhere, Category=Cinematic, AdvancedDisplay, meta=(EditCondition="bCinematicMode"))
	bool bShowHUD;
};