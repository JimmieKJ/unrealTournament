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
	UPROPERTY(config, EditAnywhere, Category=General, AdvancedDisplay, meta=(RelativePath))
	FDirectoryPath OutputDirectory;

	/** The format to use for the resulting filename. Extension will be added automatically. Any tokens of the form {token} will be replaced with the corresponding value:
	 * {fps}		- The captured framerate
	 * {frame}		- The current frame number (only relevant for image sequences)
	 * {width}		- The width of the captured frames
	 * {height}		- The height of the captured frames
	 * {world}		- The name of the current world
	 */
	UPROPERTY(config, EditAnywhere, Category=General, DisplayName="Filename Format")
	FString OutputFormat;

	/** Whether to overwrite existing files or not */
	UPROPERTY(config, EditAnywhere, Category=General)
	bool bOverwriteExisting;

	/** The frame rate at which to capture */
	UPROPERTY(config, EditAnywhere, Category=CaptureSettings)
	int32 FrameRate;

	/** An explicit number of frames to capture, or 0 if unspecified. Not exposed on UI - calculated internally */
	UPROPERTY()
	int32 FrameCount;

	/** The resolution at which to capture */
	UPROPERTY(config, EditAnywhere, Category=CaptureSettings, meta=(ShowOnlyInnerProperties))
	FCaptureResolution Resolution;

	/** The type of capture to perform */
	UPROPERTY(config, EditAnywhere, Category=CaptureSettings, AdvancedDisplay)
	EMovieCaptureType CaptureType;

	/** Whether compression is enabled on the resulting file(s) */
	UPROPERTY(config, EditAnywhere, Category=CaptureSettings)
	bool bUseCompression;

	/** The level of compression to apply to the captured file(s) (0-1) */
	UPROPERTY(config, EditAnywhere, Category=CaptureSettings, AdvancedDisplay, meta=(ClampMin=0, ClampMax=1))
	float CompressionQuality;

	/** (Experimental) - An optional codec to use for video encoding */
	UPROPERTY(config, EditAnywhere, Category=CaptureSettings, AdvancedDisplay)
	FString Codec;
	
	/** Whether to enable texture streaming whilst capturing */
	UPROPERTY(config, EditAnywhere, Category=CaptureSettings)
	bool bEnableTextureStreaming;

	/** Whether to enable cinematic mode whilst capturing */
	UPROPERTY(config, EditAnywhere, Category=Cinematic)
	bool bCinematicMode;

	/** Whether to allow player movement whilst capturing */
	UPROPERTY(config, EditAnywhere, Category=Cinematic, AdvancedDisplay)
	bool bAllowMovement;

	/** Whether to allow player rotation whilst capturing */
	UPROPERTY(config, EditAnywhere, Category=Cinematic, AdvancedDisplay)
	bool bAllowTurning;

	/** Whether to show the local player whilst capturing */
	UPROPERTY(config, EditAnywhere, Category=Cinematic, AdvancedDisplay)
	bool bShowPlayer;

	/** Whether to show the in-game HUD whilst capturing */
	UPROPERTY(config, EditAnywhere, Category=Cinematic, AdvancedDisplay)
	bool bShowHUD;
};