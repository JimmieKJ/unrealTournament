// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine.h"
#include "ModuleInterface.h"


/** 
 * Module handling default behavior for streaming pause rendering. 
 * Games can override by calling RegisterBegin/EndStreamingPauseDelegate with their own delegates.
 */
class FStreamingPauseRenderingModule
	: public IModuleInterface
{
public:	

	/** Default constructor. */
	FStreamingPauseRenderingModule();

	/**
	 * Enqueue the streaming pause to suspend rendering during blocking load.
	 */
	virtual void BeginStreamingPause( class FViewport* Viewport );

	/**
	 * Enqueue the streaming pause to resume rendering after blocking load is completed.
	 */
	virtual void EndStreamingPause();

public:

	// IModuleInterface interface

	virtual void StartupModule();	
	virtual void ShutdownModule();

public:

	TSharedPtr<FSceneViewport>		Viewport;
	TSharedPtr<SViewport>			ViewportWidget;
	FViewportClient*			ViewportClient;

	/** Delegate providing default functionality for beginning streaming pause. */
	FBeginStreamingPauseDelegate BeginDelegate;

	/** Delegate providing default functionality for ending streaming pause. */
	FEndStreamingPauseDelegate EndDelegate;
};
