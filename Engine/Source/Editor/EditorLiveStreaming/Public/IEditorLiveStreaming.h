// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ModuleManager.h"


/**
 * Interface to the EditorLiveStreaming module's functionality that enables live broadcasting of video and audio from editor sessions
 */
class IEditorLiveStreaming
	: public IModuleInterface
{

public:

	/**
	 * Checks to see if live streaming is currently available for the editor 
	 *
	 * @return	True if live streaming is possible to start
	 */
	virtual bool IsLiveStreamingAvailable() const = 0;

	/**
	 * Returns true if we're currently broadcasting
	 *
	 * @return	True if broadcasting is currently active
	 */
	virtual bool IsBroadcastingEditor() const = 0;

	/** Starts broadcasting frames */
	virtual void StartBroadcastingEditor() = 0;

	/** Stops broadcasting frames */
	virtual void StopBroadcastingEditor() = 0;

	/** Pushes latest video from the editor to a live stream.  The editor's rendering system will call this each frame. */
	virtual void BroadcastEditorVideoFrame() = 0;


public:

	/**
	 * Singleton-like access to IEditorLiveStreaming
	 *
	 * @return Returns instance of IEditorLiveStreaming object
	 */
	static inline IEditorLiveStreaming& Get()
	{
		static FName LiveStreamingModule("EditorLiveStreaming");

		return FModuleManager::LoadModuleChecked< IEditorLiveStreaming >( LiveStreamingModule );
	}

};


