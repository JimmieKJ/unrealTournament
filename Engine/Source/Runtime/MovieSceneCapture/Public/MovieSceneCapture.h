// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneCaptureSettings.h"
#include "IMovieSceneCapture.h"
#include "MovieSceneCaptureHandle.h"
#include "MovieScene.h"
#include "AVIWriter.h"
#include "Tickable.h"
#include "MovieSceneCapture.generated.h"

/** Interface that defines when to capture or drop frames */
struct ICaptureStrategy
{
	virtual ~ICaptureStrategy(){}
	
	virtual void OnWarmup() = 0;
	virtual void OnStart() = 0;
	virtual void OnStop() = 0;
	virtual void OnPresent(double CurrentTimeSeconds, uint32 FrameIndex) = 0;
	virtual bool ShouldSynchronizeFrames() const { return true; }

	virtual bool ShouldPresent(double CurrentTimeSeconds, uint32 FrameIndex) const = 0;
	virtual int32 GetDroppedFrames(double CurrentTimeSeconds, uint32 FrameIndex) const = 0;
};

/** Structure used to cache various metrics for our capture */
struct FCachedMetrics
{
	FCachedMetrics() : Width(0), Height(0), Frame(0), ElapsedSeconds(0.f) {}

	/** The width/Height of the frame */
	int32 Width, Height;
	/** The current frame number */
	int32 Frame;
	/** The number of seconds that have elapsed */
	float ElapsedSeconds;
};

/** Class responsible for capturing scene data */
UCLASS(config=EditorPerProjectUserSettings)
class MOVIESCENECAPTURE_API UMovieSceneCapture : public UObject, public IMovieSceneCaptureInterface
{
public:

	UMovieSceneCapture(const FObjectInitializer& Initializer);

	GENERATED_BODY()

public:

	// Begin IMovieSceneCaptureInterface
	virtual void Initialize(TWeakPtr<FSceneViewport> InSceneViewport) override;
	virtual void SetupFrameRange() override
	{
	}
	virtual void Close() override;
	virtual FMovieSceneCaptureHandle GetHandle() const override { return Handle; }
	const FMovieSceneCaptureSettings& GetSettings() const override { return Settings; }
	// End IMovieSceneCaptureInterface

public:

	/** Settings that define how to capture */
	UPROPERTY(EditAnywhere, config, Category=CaptureSettings, meta=(ShowOnlyInnerProperties))
	FMovieSceneCaptureSettings Settings;

	/** Additional command line arguments to pass to the external process when capturing */
	UPROPERTY(EditAnywhere, config, Category=General, AdvancedDisplay)
	FString AdditionalCommandLineArguments;

	/** Command line arguments inherited from this process */
	UPROPERTY(EditAnywhere, transient, Category=General, AdvancedDisplay)
	FString InheritedCommandLineArguments;

	/** Value used to control the BufferVisualizationDumpFrames cvar in the child process */
	UPROPERTY()
	bool bBufferVisualizationDumpFrames;

public:

	/** Access this object's cached metrics */
	const FCachedMetrics& GetMetrics() const { return CachedMetrics; }

	/** Starts warming up.  May be optionally called before StartCapture().  This can be used to start rendering frames early, before
	    any files are written out */
	void StartWarmup();

	/** Initialize the capture */
	void StartCapture();

	/** Finalize the capture, waiting for any outstanding processing */
	void StopCapture();

	/** Capture a frame from our previously populated color buffer (populated by slate) */
	void CaptureFrame(float DeltaSeconds);

protected:

#if WITH_EDITOR
	/** Implementation function that saves out a snapshot file from the specified color data */
	void SaveFrameToFile(TArray<FColor> Colors);
#endif

	/** Prepare the slate renderer for a screenshot */
	void PrepareForScreenshot();

	/** Called at the end of a frame, before a frame is presented by slate */
	virtual void Tick(float DeltaSeconds);

private:

	/** Called when this movie scene capture has stopped */
	virtual void OnCaptureStopped() {}

protected:

	/** Resolve the specified format using the user supplied formatting rules. */
	FString ResolveFileFormat(const FString& Folder, const FString& Format) const;

	/** Get the default extension required for this capture (avi, jpg etc) */
	const TCHAR* GetDefaultFileExtension() const;

	/** Calculate a unique index for the {unique} formatting rule */
	FString ResolveUniqueFilename();

private:

	struct FTicker : public FTickableGameObject
	{
		FTicker(UMovieSceneCapture* InCapture) : Capture(InCapture) {}

	private:
		virtual bool IsTickableInEditor() const override { return false; }
		virtual bool IsTickable() const override { return true; }
		virtual bool IsTickableWhenPaused() const override { return false; }
		virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UMovieSceneCapture, STATGROUP_Tickables); }
		virtual void Tick(float DeltaSeconds) override
		{
			Capture->Tick(DeltaSeconds);
		}

		UMovieSceneCapture* Capture;
	};
	friend FTicker;

protected:
	
	/** The viewport we are bound to */
	TWeakPtr<FSceneViewport> SceneViewport;
	/** Our unique handle, used for external representation without having to link to the MovieSceneCapture module */
	FMovieSceneCaptureHandle Handle;
	/** Cached metrics for this capture operation */
	FCachedMetrics CachedMetrics;
	/** Optional AVI file writer used when capturing to video */
	TUniquePtr<FAVIWriter> AVIWriter;
	/** Strategy used for capture (real-time/fixed-time-step) */
	TSharedPtr<ICaptureStrategy> CaptureStrategy;
	/** Scratch space for per-frame screenshots */
	TArray<FColor> ScratchBuffer;
	/** The number of frames to capture.  If this is zero, we'll capture the entire sequence. */
	int32 FrameCount;
	/** Running count of how many frames we're waiting to capture from slate */
	int32 OutstandingFrameCount;
	/** The delta of the last frame */
	float LastFrameDelta;
	/** Whether we have started capturing or not */
	bool bCapturing;
	/** Class responsible for ticking this instance */
	TUniquePtr<FTicker> Ticker;
	/** Frame number index offset when saving out frames.  This is used to allow the frame numbers on disk to match
	    what they would be in the authoring application, rather than a simple 0-based sequential index */
	int32 FrameNumberOffset;

};

/** A strategy that employs a fixed frame time-step, and as such never drops a frame. Potentially accelerated.  */
struct MOVIESCENECAPTURE_API FFixedTimeStepCaptureStrategy : ICaptureStrategy
{
	FFixedTimeStepCaptureStrategy(uint32 InTargetFPS);

	virtual void OnWarmup() override;
	virtual void OnStart() override;
	virtual void OnStop() override;
	virtual void OnPresent(double CurrentTimeSeconds, uint32 FrameIndex) override;	
	virtual bool ShouldPresent(double CurrentTimeSeconds, uint32 FrameIndex) const override;
	virtual int32 GetDroppedFrames(double CurrentTimeSeconds, uint32 FrameIndex) const override;

private:
	uint32 TargetFPS;
};

/** A capture strategy that captures in real-time, potentially dropping frames to maintain a stable constant framerate video. */
struct MOVIESCENECAPTURE_API FRealTimeCaptureStrategy : ICaptureStrategy
{
	FRealTimeCaptureStrategy(uint32 InTargetFPS);

	virtual void OnWarmup() override;
	virtual void OnStart() override;
	virtual void OnStop() override;
	virtual void OnPresent(double CurrentTimeSeconds, uint32 FrameIndex) override;
	virtual bool ShouldSynchronizeFrames() const override { return false; }
	virtual bool ShouldPresent(double CurrentTimeSeconds, uint32 FrameIndex) const override;
	virtual int32 GetDroppedFrames(double CurrentTimeSeconds, uint32 FrameIndex) const override;

private:
	double NextPresentTimeS, FrameLength;
};