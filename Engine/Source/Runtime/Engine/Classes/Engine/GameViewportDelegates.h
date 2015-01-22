// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Delegate type used by UGameViewportClient when a screenshot has been captured
 *
 * The first parameter is the width.
 * The second parameter is the height.
 * The third parameter is the array of bitmap data.
 * @see UGameViewportClient
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnScreenshotCaptured, int32 /*Width*/, int32 /*Height*/, const TArray<FColor>& /*Colors*/);

/**
 * Delegate type used by UGameViewportClient when a png screenshot has been captured
 *
 * The first parameter is the width.
 * The second parameter is the height.
 * The third parameter is the array of bitmap data.
 * The fourth parameter is the screen shot filename.
 * @see UGameViewportClient
 */
DECLARE_DELEGATE_FourParams(FOnPNGScreenshotCaptured, int32, int32, const TArray<FColor>&, const FString&);

/**
 * Delegate type used by UGameViewportClient when call is made to close a viewport
 *
 * The first parameter is the viewport being closed.
 * @see UGameViewportClient
 */
DECLARE_DELEGATE_OneParam(FOnCloseRequested, FViewport*);
