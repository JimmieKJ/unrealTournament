// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IGameLiveStreaming.h"

/**
 * Implements support for broadcasting gameplay to a live internet stream
 */
class FGameLiveStreaming
	: public IGameLiveStreaming
{

public:

	/** Default constructor */
	FGameLiveStreaming();

	/** IModuleInterface overrides */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** IGameLiveStreaming overrides */
	virtual bool IsBroadcastingGame() const override;
	virtual void StartBroadcastingGame( const FGameBroadcastConfig& GameBroadcastConfig ) override;
	virtual void StopBroadcastingGame() override;
	virtual void DrawSimpleWebCamVideo( UCanvas* Canvas ) override;
	virtual class UTexture2D* GetWebCamTexture( bool& bIsImageFlippedHorizontally, bool& bIsImageFlippedVertically ) override;
	virtual class ILiveStreamingService* GetLiveStreamingService() override;

protected:

	/** Called by the live streaming service when streaming status has changed or an error has occurred */
	void BroadcastStatusCallback( const struct FLiveStreamingStatus& Status );

	/** Called by the live streaming service when a chat message is received */
	void OnChatMessage( const FText& UserName, const FText& Text );

	/**
	 * Called by the Slate rendered on the game thread right after a window has been rendered, to allow us to
	 * capture frame buffer content after everything has been rendered, including debug canvas graphics
	 *
	 * @param	SlateWindow				The window that was just rendered
	 * @param	ViewportRHIPtr			The viewport we rendered this window to (cast to FViewportRHIRef*)
	 */
	void OnSlateWindowRenderedDuringBroadcasting( SWindow& SlateWindow, void* ViewportRHIPtr );

	/** Broadcasts a new video frame, only if the frame's data has finished copying from the GPU to system memory */
	void BroadcastGameVideoFrame();

	/** Starts copying the last rendered game frame from the GPU back buffer to a mapped system memory texture */
	void StartCopyingNextGameVideoFrame( const FViewportRHIRef& ViewportRHI );



private:
	/** Whether we're currently trying to broadcast */
	bool bIsBroadcasting;

	/** The live streaming service we're using.  Only valid while broadcasting. */
	class ILiveStreamingService* LiveStreamer;

	/** Static: Readback textures for asynchronously reading the viewport frame buffer back to the CPU.  We ping-pong between the buffers to avoid stalls. */
	FTexture2DRHIRef ReadbackTextures[2];
	
	/** Static: We ping pong between the textures in case the GPU is a frame behind (GSystemSettings.bAllowOneFrameThreadLag) */
	int32 ReadbackTextureIndex;

	/** Static: Pointers to mapped system memory readback textures that game frames will be asynchronously copied to */
	void* ReadbackBuffers[2];

	/** The current buffer index.  We bounce between them to avoid stalls. */
	int32 ReadbackBufferIndex;

	/** True if the user prefers to flip the web camera image horizontally */
	bool bMirrorWebCamImage;

	/** True if we should draw a simple web cam video on top of the viewport while broadcasting */
	bool bDrawSimpleWebCamVideo;

	/** Console command for starting broadcast */
	FAutoConsoleCommand BroadcastStartCommand;

	/** Console command for stopping broadcast */
	FAutoConsoleCommand BroadcastStopCommand;
};
