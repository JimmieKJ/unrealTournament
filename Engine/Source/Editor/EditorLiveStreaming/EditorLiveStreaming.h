// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IEditorLiveStreaming.h"


/**
 * Handles broadcasting of the editor to a live internet stream
 */
class FEditorLiveStreaming
	: public IEditorLiveStreaming
{

public:

	/** Default constructor */
	FEditorLiveStreaming();

	/** IModuleInterface overrides */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** IEditorLiveStreaming overrides */
	virtual bool IsLiveStreamingAvailable() const override;
	virtual bool IsBroadcastingEditor() const override;
	virtual void StartBroadcastingEditor() override;
	virtual void StopBroadcastingEditor() override;
	virtual void BroadcastEditorVideoFrame() override;

protected:

	/** Called by the live streaming service when streaming status has changed or an error has occurred */
	void BroadcastStatusCallback( const struct FLiveStreamingStatus& Status );

	/** Closes the window that shows the current status of the broadcast */
	void CloseBroadcastStatusWindow();

	/** Called by the live streaming service when we get a new chat message to display */
	void OnChatMessage( const FText& UserName, const FText& ChatMessage );


private:

	/** True if we're currently broadcasting anything */
	bool bIsBroadcasting;

	/** The live streaming implementation we're using for editor broadcasting */
	class ILiveStreamingService* LiveStreamer;

	/** Number of video frames we've submitted since we started broadcasting */
	uint32 SubmittedVideoFrameCount;

	/** A temporary pair of buffers that we write our video frames to */
	void* ReadbackBuffers[2];

	/** The current buffer index.  We bounce between them to avoid stalls. */
	int32 ReadbackBufferIndex;

	/** Persistent Slate notification for live streaming status */
	TWeakPtr< class SNotificationItem > NotificationWeakPtr;

	/** Dynamic slate image brush for our web cam texture */
	TSharedPtr< struct FSlateDynamicImageBrush > WebCamDynamicImageBrush;

	/** Window for broadcast status */
	TSharedPtr< class SWindow > BroadcastStatusWindow;
};
