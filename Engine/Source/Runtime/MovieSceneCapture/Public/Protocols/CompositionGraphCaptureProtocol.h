// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneCaptureProtocol.h"
#include "MovieSceneCaptureProtocolSettings.h"
#include "CompositionGraphCaptureProtocol.generated.h"

USTRUCT()
struct MOVIESCENECAPTURE_API FCompositionGraphCapturePasses
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Composition Graph Settings")
	TArray<FString> Value;
};

UCLASS(config=EditorPerProjectUserSettings, DisplayName="Composition Graph Options")
class MOVIESCENECAPTURE_API UCompositionGraphCaptureSettings : public UMovieSceneCaptureProtocolSettings
{
public:
	GENERATED_BODY()
	
	/**~ UMovieSceneCaptureProtocolSettings implementation */
	virtual void OnReleaseConfig(FMovieSceneCaptureSettings& InSettings) override;
	virtual void OnLoadConfig(FMovieSceneCaptureSettings& InSettings) override;

	/** A list of render passes to include in the capture. Leave empty to export all available passes. */
	UPROPERTY(config, EditAnywhere, Category="Composition Graph Options")
	FCompositionGraphCapturePasses IncludeRenderPasses;

	/** Whether to capture the frames as HDR textures (*.exr format) */
	UPROPERTY(config, EditAnywhere, Category="Composition Graph Options")
	bool bCaptureFramesInHDR;

	/** Custom post processing material to use for rendering */
	UPROPERTY(config, EditAnywhere, Category="Composition Graph Options", meta=(AllowedClasses=""))
	FStringAssetReference PostProcessingMaterial;
};

struct MOVIESCENECAPTURE_API FCompositionGraphCaptureProtocol : IMovieSceneCaptureProtocol
{
	/**~ IMovieSceneCaptureProtocol implementation */
	virtual bool Initialize(const FCaptureProtocolInitSettings& InSettings, const ICaptureProtocolHost& Host);
	virtual void CaptureFrame(const FFrameMetrics& FrameMetrics, const ICaptureProtocolHost& Host);
	virtual void Tick() override;
	virtual void Finalize() override;
	virtual bool HasFinishedProcessing() const override;
	/**~ End IMovieSceneCaptureProtocol implementation */

private:
	/** The viewport we are capturing from */
	TWeakPtr<FSceneViewport> SceneViewport;

	/** A view extension that we use to ensure we dump out the composition graph frames with the correct settings */
	TSharedPtr<struct FSceneViewExtension, ESPMode::ThreadSafe> ViewExtension;

	/** The render passes we want to export */
	TArray<FString> RenderPasses;
};