// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


DECLARE_LOG_CATEGORY_EXTERN(LogCrashTracker, Log, All);


/**
 * A buffer for holding a compressed video frame.
 */
class FCompressedDataFrame
{
public:
	FCompressedDataFrame(int InBufferSize);
	~FCompressedDataFrame();

	// The data buffer
	uint8* Data;
	// The data buffer's size
	int32 BufferSize;
	// The actual size of the compressed data
	int32 ActualSize;
};



/**
 * An asynchronous task for compressing an image.
 */
class FAsyncImageCompress : public FNonAbandonableTask
{
public:
	FAsyncImageCompress(IImageWrapperPtr InImageWrapper, class FCompressedDataFrame* InDataFrame);

	void DoWork();

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncImageCompress, STATGROUP_ThreadPoolAsyncTasks);
	}

private:
	/** The image compression algorithm to use */
	IImageWrapperPtr ImageWrapper;
	/** The data frame we are dumping our data out to */
	FCompressedDataFrame* DataFrame;
};



/**
 * The crash tracker handles slate key logging and if CRASH_TRACKER_ENABLED, also crash video capture.
 */
class FCrashVideoCapture
{
public:
	FCrashVideoCapture();
	~FCrashVideoCapture();
	
	/** Initiates capture of video frames */
	void BeginCapture(class FSlateRenderer* SlateRenderer);

	/** Ends capture of video frames */
	void EndCapture(bool bSaveCapture);

	/** Updates the crash tracker, which may trigger the capture of a frame */
	void Update(float DeltaSeconds);

	/** Cleans up the final frame for capture, forces all async threads to sync */
	void CleanupFinalFrame();
	
	/** Enables or disables crash tracking */
	void SetCrashTrackingEnabled(bool bEnabled);

	/** Whether or not the crash tracker is capturing anything */
	bool IsCapturing() const;

	void InvalidateFrame();

	/** Sends a key press to the crash tracker */
	void BufferKeyPress(const FString& KeyPress);

	/** 
	* Force a save of the current capture buffer
	*
	* @param	CapturePath	filepath to save to	 
	* @returns	Returns true if video capture file was successfully written
	*/
	bool SaveCaptureNow( const FString& CapturePath );

	/** The path that we output to */
	static FString CaptureVideoPath;

private:
	/** Clears up all key presses we have buffered */
	void CleanupKeyPressBuffer(float DeltaSeconds);
	
	/**  
	* Saves the final video capture
	*
	* @param	CapturePath	filepath to save to
	*/
	void SaveFinalCapture( const FString& CapturePath );

	/** Syncs up the asynchronous image compression thread */
	void SyncWorkerThread();

private:
	/** A temporary pair of buffers that we write our video frames to */
	void* Buffer[2];
	/** We ping pong between the buffers in case the GPU is a frame behind (GSystemSettings.bAllowOneFrameThreadLag) */
	int32 CurrentBufferIndex;
	/** The width and height of these frames */
	int32 Width;
	int32 Height;
	/** The slate renderer we are getting our frames from */
	FSlateRenderer* CaptureSlateRenderer;

	/** The current seconds accumulated from each Tick */
	float CurrentAccumSeconds;
	/** The current frame index that we are capturing to */
	int CurrentFrameCaptureIndex;
	/** Whether or not the video capture is running or not, this is an int32 for the autoconsolevariableref */
	int32 bIsRunning;
	/** A buffer of all the key presses and their associated time to death */
	TArray< TKeyValuePair<FString, float> > KeypressBuffer;
	
	/** A ring buffer of all the compressed frames we have */
	TArray<FCompressedDataFrame*> CompressedFrames;
	/** The asynchronous task for compressing a single frame */
	FAsyncTask<FAsyncImageCompress>* AsyncTask;

	/** Console variable for toggling whether the capture is running */
	FAutoConsoleVariableRef CVarCrashTrackerIsRunning;

	/** Ensures only one thread will ever sync up with the image compression worker thread */
	FCriticalSection SyncWorkerThreadSection;
};
