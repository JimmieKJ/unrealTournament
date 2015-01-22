// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


namespace EWriteUserCaptureVideoError
{
	/**
	 * Enumerates video capture write error codes.
	 */
	enum Type
	{
		None,

		/** The video capture instance was invalid. */
		VideoCaptureInvalid,

		/** Video capture is not running. */
		CaptureNotRunning,

		/** Failed to create destination directory for the video. */
		FailedToCreateDirectory,
	};
}


/**
 * Interface for crash tracker modules.
 */
class ICrashTrackerModule
	: public IModuleInterface
{
public:

	/**
	 * Forces the crash tracker to complete capturing as if the program had crashed.
	 */
	virtual void ForceCompleteCapture() = 0;

	/**
	 * Invalidates the current crash tracker frame being generated.
	 */
	virtual void InvalidateCrashTrackerFrame() = 0;

	/**
	 * Checks if the crash tracker is currently enabled.
	 */
	virtual bool IsCurrentlyCapturing() const = 0;

	/**
	 * Checks if the crash tracker is available.
	 */
	virtual bool IsVideoCaptureAvailable() const = 0;
	
	/**
	 * Enables or disables crash tracking while in flight.
	 */
	virtual void SetCrashTrackingEnabled(bool bEnabled) = 0;

	/**
	 * Updates the crash tracker, which may trigger the capture of a frame.
	 * Will also begin capturing if it hasn't begun already.
	 */
	virtual void Update(float DeltaSeconds) = 0;

	/**
	 * Write the current crash tracker as a user video.
	 *
	 * @param OutFinalSaveName	The path and name of the file the video was saved as.
	 * @return If the video was successfully written will return EWriteUserCaptureVideoError::None, otherwise returns an error code.
	 */
	virtual EWriteUserCaptureVideoError::Type WriteUserVideoNow( FString& OutFinalSaveName ) = 0;

public:

	/**
	 * Virtual destructor.
	 */
	virtual ~ICrashTrackerModule( ) { }
};
