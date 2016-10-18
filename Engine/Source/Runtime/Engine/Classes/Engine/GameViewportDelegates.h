// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
 * Delegate type used by UGameViewportClient when call is made to close a viewport
 *
 * The first parameter is the viewport being closed.
 * @see UGameViewportClient
 */
DECLARE_DELEGATE_OneParam(FOnCloseRequested, FViewport*);

/**
 * Delegate type used by UGameViewportClient for when a player is added or removed
 *
 * The first parameter is the player index
 * @see UGameViewportClient
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameViewportClientPlayerAction, int32);

/**
 * Delegate type used by UGameViewportClient for tick callbacks
 *
 * The first parameter is the time delta
 * @see UGameViewportClient
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGameViewportTick, float);
DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnGameViewportInputKey, FKey, FModifierKeysState);
