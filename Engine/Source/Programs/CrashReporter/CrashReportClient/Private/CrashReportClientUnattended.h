// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CrashUpload.h"

/**
 * Implementation of the crash report client used for unattended uploads
 */
class FCrashReportClientUnattended
{
public:
	/**
	 * Set up uploader object
	 * @param ErrorReport Error report to upload
	 */
	explicit FCrashReportClientUnattended(const FPlatformErrorReport& ErrorReport);

private:
	/**
	 * Update received every second
	 * @param DeltaTime Time since last update, unused
	 * @return Whether the updates should continue
	 */
	bool Tick(float DeltaTime);

	/**
	 * Begin calling Tick once a second
	 */
	void StartTicker();

	/** Object that uploads report files to the server */
	FCrashUpload Uploader;
};
