// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "WindowsHWrapper.h"
#include "AllowWindowsPlatformTypes.h"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmsystem.h>
#include "HideWindowsPlatformTypes.h"

#include "Core.h"
#include "UnrealTournament.h"

#include "UTVideoRecordingFeature.h"

// Vorbis would like special alignment
#pragma pack(push, 8)
#include "vorbis/vorbisenc.h"
#pragma pack(pop)

#include "WebMRecord.generated.h"

// Can help to have a UObject around if we want one
UCLASS(Blueprintable, Meta = (ChildCanTick))
class AWebMRecord : public AActor
{
	GENERATED_UCLASS_BODY()
	
};

class FCaptureAudioWorker : public FRunnable
{
	FCaptureAudioWorker();

	~FCaptureAudioWorker()
	{
		delete Thread;
		Thread = nullptr;
	}

	uint32 Run();

private:
	FRunnableThread* Thread;
	static FCaptureAudioWorker* Runnable;

	// Audio stuff
	IMMDevice* MMDevice;
	IAudioClient* AudioClient;
	IAudioCaptureClient* AudioCaptureClient;
	WAVEFORMATEX* WFX;
	int32 AudioBlockAlign;
	uint32 AudioTotalFrames;
	bool bCapturingAudio;
	uint64 StartingAudioTime;
	uint64 LastAudioTime;
	HMMIO hAudioFile;
	MMCKINFO ckRIFF;
	MMCKINFO ckData;
	REFERENCE_TIME hnsDefaultDevicePeriod;
	void InitAudioLoopback();
	void StopAudioLoopback();
	void ReadAudioLoopback();
	void OpenTempWaveFile();
	void CloseTempWaveFile();

public:
	bool bStopCapture;

	void WaitForCompletion()
	{
		Thread->WaitForCompletion();
	}

	static FCaptureAudioWorker* RunWorkerThread()
	{
		if (Runnable)
		{
			delete Runnable;
			Runnable = nullptr;
		}

		Runnable = new FCaptureAudioWorker();

		return Runnable;
	}
};

struct FWebMRecord : FTickableGameObject, FSelfRegisteringExec, UTVideoRecordingFeature, IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	FWebMRecord();
	virtual void Tick(float DeltaTime);
	virtual bool IsTickable() const { return true; }
	virtual bool IsTickableInEditor() const { return true; }
	virtual bool IsTickableWhenPaused() const { return true; }

	// Put a real stat id here
	virtual TStatId GetStatId() const
	{
		return TStatId();
	}

	/** FSelfRegisteringExec implementation */
	virtual bool Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	void OnWorldCreated(UWorld* World, const UWorld::InitializationValues IVS);
	void OnWorldDestroyed(UWorld* World);
	
	virtual void StartRecording(float RecordTime) override;
	virtual void CancelRecording() override
	{
		StopRecording();
	}

	void StopRecording();

	FRecordingComplete OnRecordingCompleteEvent;
	virtual FRecordingComplete& OnRecordingComplete() override
	{
		return OnRecordingCompleteEvent;
	}

	FCompressingComplete OnCompressingCompleteEvent;
	virtual FCompressingComplete& OnCompressingComplete() override
	{
		return OnCompressingCompleteEvent;
	}

	virtual void StartCompressing(const FString& Filename) override;
	virtual void CancelCompressing() override;

	float CompressionCompletionPercent;
	virtual float GetCompressionCompletionPercent() override
	{
		return CompressionCompletionPercent;
	}

	bool bCancelCompressing;
	bool bCompressionComplete;
	bool bCompressionSuccessful;
	void EncodeVideoAndAudio(const FString& Filename);
	
	void EncodeAnimatedGIF(const FString& Filename);

	virtual bool IsRecording(uint32& TickRate) override;

protected:
	bool bRecording;
	bool bCompressing;

	uint64 LastFrameCounter;
	int32 VideoWidth;
	int32 VideoHeight;
	uint32 VideoFrameRate;
	int32 VideoFramesCaptured;
	float VideoFrameDelay;
	float TotalVideoTime;
	float TimeLeftToRecord;
	double VideoRecordStart;
	double VideoRecordFirstFrame;
	double VideoRecordPreviousFrame;
	float VideoRecordLagTime;

	bool bWriteYUVToTempFile;
	TArray<uint8> YPlaneTemp;
	TArray<uint8> UPlaneTemp;
	TArray<uint8> VPlaneTemp;
	FArchive* VideoTempFile;
	void OpenTempFrameFile();
	void CloseTempFrameFile();

	// Slate texture capture stuff
	/** Static: Readback textures for asynchronously reading the viewport frame buffer back to the CPU.  We ping-pong between the buffers to avoid stalls. */
	FTexture2DRHIRef ReadbackTextures[2];
	/** Static: We ping pong between the textures in case the GPU is a frame behind (GSystemSettings.bAllowOneFrameThreadLag) */
	int32 ReadbackTextureIndex;
	/** Static: Pointers to mapped system memory readback textures that game frames will be asynchronously copied to */
	void* ReadbackBuffers[2];
	/** The current buffer index.  We bounce between them to avoid stalls. */
	int32 ReadbackBufferIndex;
	void OnSlateWindowRenderedDuringCapture(SWindow& SlateWindow, void* ViewportRHIPtr);
	void SaveCurrentFrameToDisk();
	void WriteFrameToTempFile();
	void StartCopyingNextGameFrame(const FViewportRHIRef& ViewportRHI);
	bool bRegisteredSlateDelegate;

	FCaptureAudioWorker* AudioWorker;
	class FCompressWorker* CompressWorker;
	
	// Returns a buffer that you must delete [] later
	void MakeAudioPrivateData(const ogg_packet& header, const ogg_packet& header_comm, const ogg_packet& header_code, uint8** AudioHeader, uint64& AudioHeaderLength);
	int32 XiphLacingLength(int32 ValueToLace);
	void XiphLace(uint8** BufferToLaceInto, uint64 ValueToLace);

	void DebugWriteAudioToOGG();
	void DebugUploadLastVideo(UWorld* InWorld);

	void UnmapReadbackTextures();
};

class FCompressWorker : public FRunnable
{
public:
	virtual void WaitForCompletion() = 0;
};

class FCompressVideoWorker : public FCompressWorker
{
	FCompressVideoWorker(FWebMRecord* InWebMRecorder, const FString& InFilename);

	~FCompressVideoWorker()
	{
		delete Thread;
		Thread = nullptr;
	}

	uint32 Run();

public:
	bool bStopCapture;

	void WaitForCompletion()
	{
		Thread->WaitForCompletion();
	}

	static FCompressVideoWorker* RunWorkerThread(FWebMRecord* InWebMRecorder, const FString& InFilename)
	{
		if (Runnable)
		{
			delete Runnable;
			Runnable = nullptr;
		}

		Runnable = new FCompressVideoWorker(InWebMRecorder, InFilename);

		return Runnable;
	}
private:
	FRunnableThread* Thread;
	FWebMRecord* WebMRecorder;
	FString Filename;
	static FCompressVideoWorker* Runnable;
};


class FCreateAnimatedGIFWorker : public FCompressWorker
{
	FCreateAnimatedGIFWorker(FWebMRecord* InWebMRecorder, const FString& InFilename);

	~FCreateAnimatedGIFWorker()
	{
		delete Thread;
		Thread = nullptr;
	}

	uint32 Run();

public:
	bool bStopCapture;

	void WaitForCompletion()
	{
		Thread->WaitForCompletion();
	}

	static FCreateAnimatedGIFWorker* RunWorkerThread(FWebMRecord* InWebMRecorder, const FString& InFilename)
	{
		if (Runnable)
		{
			delete Runnable;
			Runnable = nullptr;
		}

		Runnable = new FCreateAnimatedGIFWorker(InWebMRecorder, InFilename);

		return Runnable;
	}
private:
	FRunnableThread* Thread;
	FWebMRecord* WebMRecorder;
	FString Filename;
	static FCreateAnimatedGIFWorker* Runnable;
};
