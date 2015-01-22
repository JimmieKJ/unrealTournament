// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AVIWriter.h: Helper class for creating AVI files.
=============================================================================*/

#ifndef _AVIWRITER_HEADER_
#define _AVIWRITER_HEADER_

DECLARE_LOG_CATEGORY_EXTERN(LogMovieCapture, Warning, All);

/** An event for synchronizing direct show encoding with the game thread */
extern FEvent* GCaptureSyncEvent;

class FAVIWriter
{
protected:
	/** Protected constructor to avoid abuse. */
	FAVIWriter() 
		: ColorBuffer(NULL)
		, MovieCaptureIndex(0)
		, bCapturing(false)
		, bReadyForCapture(false)
		, bMatineeFinished(false)
		, CaptureViewport(NULL)
		, CaptureSlateRenderer(NULL)
		, FrameNumber(0)
		, CurrentAccumSeconds( 0.f )
	{

	}

	FColor* ColorBuffer;
	int32 MovieCaptureIndex;
	bool bCapturing;
	bool bReadyForCapture;
	bool bMatineeFinished;
	TArray< FColor > ViewportColorBuffer;
	FViewport* CaptureViewport;
	class FSlateRenderer* CaptureSlateRenderer;
	int32 FrameNumber;
	float CurrentAccumSeconds;

public:
	ENGINE_API static FAVIWriter* GetInstance();

	uint32 GetWidth()
	{
		return CaptureViewport ? CaptureViewport->GetSizeXY().X : 0;
	}

	uint32 GetHeight()
	{
		return CaptureViewport ? CaptureViewport->GetSizeXY().Y : 0;
	}

	int32 GetFrameNumber()
	{
		return FrameNumber;
	}

	bool IsCapturing()
	{
		return bCapturing;
	}

	bool IsCapturedMatineeFinished()
	{
		return bMatineeFinished;
	}

	bool IsCapturingSlateRenderer()
	{
		return CaptureSlateRenderer != NULL;
	}

	void SetCapturedMatineeFinished(bool finished = true)
	{
		bMatineeFinished = finished;
	}

	FViewport* GetViewport()
	{
		return CaptureViewport;
	}

	const TArray<FColor>& GetColorBuffer()
	{
		return ViewportColorBuffer;
	}

	virtual void StartCapture(FViewport* Viewport = NULL, const FString& OutputPath = FString()) = 0;
	virtual void StopCapture() = 0;
	virtual void Close() = 0;
	virtual void Update(float DeltaSeconds) = 0;

};
#endif	//#ifndef _AVIWRITER_HEADER_