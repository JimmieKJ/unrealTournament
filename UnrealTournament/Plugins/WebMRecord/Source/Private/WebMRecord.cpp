// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WebMRecord.h"
#include "UnrealTournament.h"
#include "UTPlayerController.h"
#include "ModuleInterface.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "SlateBasics.h"
#include "ScreenRendering.h"
#include "RenderCore.h"
#include "RHIStaticStates.h"
#include "RendererInterface.h"

#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#endif

#pragma push_macro("RESTRICT")
#undef RESTRICT

#if PLATFORM_32BITS
#include "vpx32/vpx_encoder.h"
#include "vpx32/vp8cx.h"
#include "vpx32/video_writer.h"
#include "vpx32/webmenc.h"
#include "vpx32/vpx_mem.h"
#else
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
#include "vpx/video_writer.h"
#include "vpx/webmenc.h"
#include "vpx/vpx_mem.h"
#endif

#include "libyuv/convert.h"

#include <Avrt.h>

#include "libgd/gd.h"

#pragma pop_macro("RESTRICT")

#include "UTGameViewportClient.h"

IMPLEMENT_MODULE(FWebMRecord, WebMRecord)

DEFINE_LOG_CATEGORY_STATIC(LogUTWebM, Log, All);

#include "AllowWindowsPlatformTypes.h"

AWebMRecord::AWebMRecord(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

FWebMRecord::FWebMRecord()
{
	bRecording = false;
	bCompressing = false;
	bCompressionComplete = false;
	bCancelCompressing = false;
	bCompressionSuccessful = false;
	CompressWorker = nullptr;
	
	VideoWidth = 1280;
	VideoHeight = 720;

	VideoFrameRate = 30;
	VideoFrameDelay = 1.0f / VideoFrameRate;
	TotalVideoTime = 0;
	TimeLeftToRecord = -1;
	VideoRecordStart = 0;
	VideoRecordFirstFrame = -1;

	bRegisteredSlateDelegate = false;
	ReadbackTextureIndex = 0;
	ReadbackBufferIndex = 0;
	ReadbackBuffers[0] = nullptr;
	ReadbackBuffers[1] = nullptr;

	VideoTempFile = nullptr;

	bWriteYUVToTempFile = true;

	LastFrameCounter = 0;
}

void FWebMRecord::StartupModule()
{
	FWorldDelegates::FWorldInitializationEvent::FDelegate OnWorldCreatedDelegate = FWorldDelegates::FWorldInitializationEvent::FDelegate::CreateRaw(this, &FWebMRecord::OnWorldCreated);
	FDelegateHandle OnWorldCreatedDelegateHandle = FWorldDelegates::OnPostWorldInitialization.Add(OnWorldCreatedDelegate);

	IModularFeatures::Get().RegisterModularFeature(TEXT("VideoRecording"), this);
}

void FWebMRecord::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(TEXT("VideoRecording"), this);

	FWorldDelegates::FWorldEvent::FDelegate OnWorldDestroyedDelegate = FWorldDelegates::FWorldEvent::FDelegate::CreateRaw(this, &FWebMRecord::OnWorldDestroyed);
	FDelegateHandle OnWorldDestroyedDelegateHandle = FWorldDelegates::OnPreWorldFinishDestroy.Add(OnWorldDestroyedDelegate);
}

void FWebMRecord::OnWorldCreated(UWorld* World, const UWorld::InitializationValues IVS)
{
	if (IsRunningCommandlet() || IsRunningDedicatedServer())
	{
		return;
	}

	if (!bRegisteredSlateDelegate)
	{
		FSlateRenderer* SlateRenderer = FSlateApplication::Get().GetRenderer().Get();
		SlateRenderer->OnSlateWindowRendered().AddRaw(this, &FWebMRecord::OnSlateWindowRenderedDuringCapture);
		bRegisteredSlateDelegate = true;

		// Setup readback buffer textures
		{
			for (int32 TextureIndex = 0; TextureIndex < 2; ++TextureIndex)
			{
				ReadbackTextures[TextureIndex] = nullptr;
			}

			ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
				FWebMRecordCreateBufers,
				int32, InVideoWidth, VideoWidth,
				int32, InVideoHeight, VideoHeight,
				FTexture2DRHIRef*, InReadbackTextures, ReadbackTextures,
			{
				for (int32 TextureIndex = 0; TextureIndex < 2; ++TextureIndex)
				{
					FRHIResourceCreateInfo CreateInfo;
					InReadbackTextures[TextureIndex] = RHICreateTexture2D(
						InVideoWidth,
						InVideoHeight,
						PF_B8G8R8A8,
						1,
						1,
						TexCreate_CPUReadback,
						CreateInfo
						);
				}
			});
			FlushRenderingCommands();

			for (int32 TextureIndex = 0; TextureIndex < 2; ++TextureIndex)
			{
				check(ReadbackTextures[TextureIndex].GetReference());
			}

			ReadbackTextureIndex = 0;

			ReadbackBuffers[0] = nullptr;
			ReadbackBuffers[1] = nullptr;
			ReadbackBufferIndex = 0;
		}
	}
}

void FWebMRecord::OnWorldDestroyed(UWorld* World)
{
	if (IsRunningCommandlet() || IsRunningDedicatedServer())
	{
		return;
	}

	// Might want to track if the world we're recording just got destroyed, but really doesn't stop this train
}

void FWebMRecord::StartRecording(float RecordTime)
{
	bRecording = true;
	TotalVideoTime = 0;
	TimeLeftToRecord = RecordTime;
	ReadbackBuffers[0] = nullptr;
	ReadbackBuffers[1] = nullptr;

	VideoFramesCaptured = 0;
	VideoRecordStart = FApp::GetLastTime();
	VideoRecordFirstFrame = -1;
	VideoRecordLagTime = 0;
	VideoRecordPreviousFrame = 0;

	OpenTempFrameFile();

	// Non-threaded audio capture does not work well
	AudioWorker = FCaptureAudioWorker::RunWorkerThread();
}

void FWebMRecord::StopRecording()
{
	bRecording = false;

	CloseTempFrameFile();
	UnmapReadbackTextures();

	AudioWorker->bStopCapture = true;
	AudioWorker->WaitForCompletion();

	UE_LOG(LogUTWebM, Log, TEXT("Recording finished, %f seconds, %d video frames, %f video lag time"), TotalVideoTime, VideoFramesCaptured, VideoRecordFirstFrame - VideoRecordStart);

	OnRecordingCompleteEvent.Broadcast();
}

bool FWebMRecord::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("VIDEORECORD")))
	{
		if (!bRecording)
		{
			StartRecording(-1);
		}
		else
		{
			StopRecording();
		}

		return true;
	}
	// DEBUG: test muxing the audio and video from raw source
	else if (FParse::Command(&Cmd, TEXT("VIDEOMUX")))
	{
		EncodeVideoAndAudio(TEXT(""));

		return true;
	}
	// DEBUG: upload the last video written to youtube
	else if (FParse::Command(&Cmd, TEXT("UPLOADVIDEO")))
	{
		DebugUploadLastVideo(InWorld);
		return true;
	}

	return false;
}

void FWebMRecord::Tick(float DeltaTime)
{
	if (GIsEditor)
	{
		return;
	}

	// Avoid double ticking
	if (LastFrameCounter > 0 && LastFrameCounter == GFrameCounter)
	{
		return;
	}

	LastFrameCounter = GFrameCounter;

	if (bRecording)
	{
		TotalVideoTime += DeltaTime;

		if (TimeLeftToRecord > 0)
		{
			TimeLeftToRecord -= DeltaTime;
			if (TimeLeftToRecord <= 0)
			{
				StopRecording();
			}
		}
	}

	if (bCompressing)
	{
		if (bCompressionComplete)
		{
			bCompressing = false;
			OnCompressingComplete().Broadcast(bCompressionSuccessful);
		}
	}
}

void FWebMRecord::OnSlateWindowRenderedDuringCapture(SWindow& SlateWindow, void* ViewportRHIPtr)
{
	// Probably need to verify that this is the window we want, but not sure yet

	UGameViewportClient* GameViewportClient = GEngine->GameViewport;
	if (bRecording && GameViewportClient != nullptr)
	{
		if (GameViewportClient->GetWindow() == SlateWindow.AsShared())
		{
			//UE_LOG(LogUTWebM, Log, TEXT("Saving video frame %f"), VideoDeltaTimeAccum);
			const FViewportRHIRef* ViewportRHI = (const FViewportRHIRef*)ViewportRHIPtr;
			StartCopyingNextGameFrame(*ViewportRHI);
			
			SaveCurrentFrameToDisk();
		}
	}
}

void FWebMRecord::OpenTempFrameFile()
{
	FString BasePath = FPaths::GameSavedDir();
	FString SavePath = BasePath / TEXT("frames.raw");
	VideoTempFile = IFileManager::Get().CreateFileWriter(*SavePath);

}

void FWebMRecord::CloseTempFrameFile()
{
	if (VideoTempFile)
	{
		VideoTempFile->Close();
		VideoTempFile = nullptr;
	}
}

void FWebMRecord::WriteFrameToTempFile()
{
	if (VideoTempFile)
	{
		if (bWriteYUVToTempFile)
		{
			if (YPlaneTemp.Num() < VideoWidth * VideoHeight)
			{
				YPlaneTemp.Empty(VideoWidth * VideoHeight);
				YPlaneTemp.AddZeroed(VideoWidth * VideoHeight);
			}
			if (UPlaneTemp.Num() < VideoWidth * VideoHeight / 4)
			{
				UPlaneTemp.Empty(VideoWidth * VideoHeight / 4);
				UPlaneTemp.AddZeroed(VideoWidth * VideoHeight / 4);
			}
			if (VPlaneTemp.Num() < VideoWidth * VideoHeight / 4)
			{
				VPlaneTemp.Empty(VideoWidth * VideoHeight / 4);
				VPlaneTemp.AddZeroed(VideoWidth * VideoHeight / 4);
			}
			// Use libyuv to convert from ARGB to YUV
			libyuv::ARGBToI420((const uint8*)ReadbackBuffers[ReadbackBufferIndex], VideoWidth * 4,
				YPlaneTemp.GetData(), VideoWidth,
				UPlaneTemp.GetData(), VideoWidth / 2,
				VPlaneTemp.GetData(), VideoWidth / 2, VideoWidth, VideoHeight);

			VideoTempFile->Serialize(YPlaneTemp.GetData(), VideoWidth * VideoHeight);
			VideoTempFile->Serialize(UPlaneTemp.GetData(), VideoWidth * VideoHeight / 4);
			VideoTempFile->Serialize(VPlaneTemp.GetData(), VideoWidth * VideoHeight / 4);
		}
		else
		{
			VideoTempFile->Serialize(ReadbackBuffers[ReadbackBufferIndex], VideoWidth * VideoHeight * sizeof(FColor));
		}
	}
}

void FWebMRecord::SaveCurrentFrameToDisk()
{
	if (ReadbackBuffers[ReadbackBufferIndex] != nullptr)
	{
		// Have a new buffer from the GPU

		// Write this frame to disk
		WriteFrameToTempFile();

		// Make up for video lag when possible by writing the frame over
		while (VideoRecordLagTime > VideoFrameDelay)
		{
			WriteFrameToTempFile();
			//UE_LOG(LogUTWebM, Log, TEXT("Wrote extra frame"));
			VideoRecordLagTime -= VideoFrameDelay;
		}

		if (VideoRecordFirstFrame < 0)
		{
			VideoRecordFirstFrame = FApp::GetLastTime();
		}
		
		if (VideoRecordPreviousFrame > 0)
		{
			double FrameLength = FApp::GetLastTime() - VideoRecordPreviousFrame;
			//UE_LOG(LogUTWebM, Log, TEXT("Actual video delay %f"), FrameLength);
			VideoRecordLagTime += FrameLength - VideoFrameDelay;
		}
		VideoRecordPreviousFrame = FApp::GetLastTime();

		// If we get hung, just write the same frame over so that audio will match
		VideoFramesCaptured++;

		// Unmap the buffer now that we've pushed out the frame
		{
			struct FReadbackFromStagingBufferContext
			{
				FWebMRecord* This;
			};
			FReadbackFromStagingBufferContext ReadbackFromStagingBufferContext =
			{
				this
			};
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				ReadbackFromStagingBuffer,
				FReadbackFromStagingBufferContext, Context, ReadbackFromStagingBufferContext,
				{
				RHICmdList.UnmapStagingSurface(Context.This->ReadbackTextures[Context.This->ReadbackTextureIndex]);
			});
		}
	}
}

void FWebMRecord::UnmapReadbackTextures()
{
	if (ReadbackBuffers[0] != nullptr)
	{
		struct FReadbackFromStagingBufferContext
		{
			FWebMRecord* This;
		};
		FReadbackFromStagingBufferContext ReadbackFromStagingBufferContext =
		{
			this
		};
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			ReadbackFromStagingBuffer,
			FReadbackFromStagingBufferContext, Context, ReadbackFromStagingBufferContext,
			{
			RHICmdList.UnmapStagingSurface(Context.This->ReadbackTextures[0]);
		});
	}
	if (ReadbackBuffers[1] != nullptr)
	{
		struct FReadbackFromStagingBufferContext
		{
			FWebMRecord* This;
		};
		FReadbackFromStagingBufferContext ReadbackFromStagingBufferContext =
		{
			this
		};
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			ReadbackFromStagingBuffer,
			FReadbackFromStagingBufferContext, Context, ReadbackFromStagingBufferContext,
			{
			RHICmdList.UnmapStagingSurface(Context.This->ReadbackTextures[1]);
		});
	}

	ReadbackTextureIndex = 0;
	ReadbackBufferIndex = 0;
}

void FWebMRecord::StartCopyingNextGameFrame(const FViewportRHIRef& ViewportRHI)
{
	// There has to be a better way to get the back buffer data.

	const FIntPoint ResizeTo(VideoWidth, VideoHeight);

	static const FName RendererModuleName("Renderer");
	IRendererModule& RendererModule = FModuleManager::GetModuleChecked<IRendererModule>(RendererModuleName);

	UGameViewportClient* GameViewportClient = GEngine->GameViewport;
	check(GameViewportClient != nullptr);

	struct FCopyVideoFrame
	{
		FViewportRHIRef ViewportRHI;
		IRendererModule* RendererModule;
		FIntPoint ResizeTo;
		FWebMRecord* This;
	};
	FCopyVideoFrame CopyVideoFrame =
	{
		ViewportRHI,
		&RendererModule,
		ResizeTo,
		this
	};

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReadSurfaceCommand,
		FCopyVideoFrame, Context, CopyVideoFrame,
		{
		FPooledRenderTargetDesc OutputDesc(FPooledRenderTargetDesc::Create2DDesc(Context.ResizeTo, PF_B8G8R8A8, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));

		const auto FeatureLevel = GMaxRHIFeatureLevel;

		TRefCountPtr<IPooledRenderTarget> ResampleTexturePooledRenderTarget;
		Context.RendererModule->RenderTargetPoolFindFreeElement(RHICmdList, OutputDesc, ResampleTexturePooledRenderTarget, TEXT("ResampleTexture"));
		check(ResampleTexturePooledRenderTarget);

		const FSceneRenderTargetItem& DestRenderTarget = ResampleTexturePooledRenderTarget->GetRenderTargetItem();

		SetRenderTarget(RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
		RHICmdList.SetViewport(0, 0, 0.0f, Context.ResizeTo.X, Context.ResizeTo.Y, 1.0f);

		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
		RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

		FTexture2DRHIRef ViewportBackBuffer = RHICmdList.GetViewportBackBuffer(Context.ViewportRHI);

		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

		static FGlobalBoundShaderState BoundShaderState;
		SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, Context.RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI, *VertexShader, *PixelShader);

		if (Context.ResizeTo != FIntPoint(ViewportBackBuffer->GetSizeX(), ViewportBackBuffer->GetSizeY()))
		{
			// We're scaling down the window, so use bilinear filtering
			PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), ViewportBackBuffer);
		}
		else
		{
			// Drawing 1:1, so no filtering needed
			PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), ViewportBackBuffer);
		}

		Context.RendererModule->DrawRectangle(
			RHICmdList,
			0, 0,		// Dest X, Y
			Context.ResizeTo.X, Context.ResizeTo.Y,	// Dest Width, Height
			0, 0,		// Source U, V
			1, 1,		// Source USize, VSize
			Context.ResizeTo,		// Target buffer size
			FIntPoint(1, 1),		// Source texture size
			*VertexShader,
			EDRF_Default);

		// Asynchronously copy render target from GPU to CPU
		const bool bKeepOriginalSurface = false;
		RHICmdList.CopyToResolveTarget(
			DestRenderTarget.TargetableTexture,
			Context.This->ReadbackTextures[Context.This->ReadbackTextureIndex],
			bKeepOriginalSurface,
			FResolveParams());
	});


	// Start mapping the newly-rendered buffer
	{
		struct FReadbackFromStagingBufferContext
		{
			FWebMRecord* This;
		};
		FReadbackFromStagingBufferContext ReadbackFromStagingBufferContext =
		{
			this
		};
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			ReadbackFromStagingBuffer,
			FReadbackFromStagingBufferContext, Context, ReadbackFromStagingBufferContext,
			{
			int32 UnusedWidth = 0;
			int32 UnusedHeight = 0;
			RHICmdList.MapStagingSurface(Context.This->ReadbackTextures[Context.This->ReadbackTextureIndex], Context.This->ReadbackBuffers[Context.This->ReadbackBufferIndex], UnusedWidth, UnusedHeight);

			// Ping pong between readback textures
			Context.This->ReadbackTextureIndex = (Context.This->ReadbackTextureIndex + 1) % 2;
		});
	}

	// Ping pong between readback buffers
	ReadbackBufferIndex = (ReadbackBufferIndex + 1) % 2;
}

int32 FWebMRecord::XiphLacingLength(int32 ValueToLace)
{
	return 1 + ValueToLace / 255 + ValueToLace;
}

void FWebMRecord::XiphLace(uint8** BufferToLaceInto, uint64 ValueToLace)
{
	uint8* LaceIterator = *BufferToLaceInto;

	while (ValueToLace >= 255)
	{
		*LaceIterator = 255;
		ValueToLace -= 255;

		LaceIterator += 1;
	}

	*LaceIterator = ValueToLace;

	LaceIterator += 1;
	*BufferToLaceInto = LaceIterator;
}

void FWebMRecord::MakeAudioPrivateData(const ogg_packet& header, const ogg_packet& header_comm, const ogg_packet& header_code, uint8** AudioHeader, uint64& AudioHeaderLength)
{
	// XIPH lacing
	// http://www.matroska.org/technical/specs/index.html

	AudioHeaderLength = 1 + XiphLacingLength(header.bytes) + XiphLacingLength(header_comm.bytes) + header_code.bytes;
	*AudioHeader = new uint8[AudioHeaderLength];
	uint8* HeaderIterator = *AudioHeader;

	// Need documentation link on this format
	*HeaderIterator = 2;
	HeaderIterator += 1;
	XiphLace(&HeaderIterator, header.bytes);
	XiphLace(&HeaderIterator, header_comm.bytes);

	FMemory::Memcpy(HeaderIterator, header.packet, header.bytes);
	HeaderIterator += header.bytes;

	FMemory::Memcpy(HeaderIterator, header_comm.packet, header_comm.bytes);
	HeaderIterator += header_comm.bytes;

	FMemory::Memcpy(HeaderIterator, header_code.packet, header_code.bytes);
}


void* VPXMalloc(size_t a)
{
	return FMemory::Malloc(a);
}

void* VPXCalloc(size_t a, size_t b)
{
	void* NewMemory = FMemory::Malloc(a, b);
	if (NewMemory)
	{
		FMemory::Memset(NewMemory, 0, a);
	}

	return NewMemory;
}

void* VPXRealloc(void* OldMemory, size_t a)
{
	return FMemory::Realloc(OldMemory, a);
}

void VPXFree(void* OldMemory)
{
	return FMemory::Free(OldMemory);
}

void* VPXMemcpy(void* a, const void* b, size_t c)
{
	return FMemory::Memcpy(a, b, c);
}

void* VPXMemset(void* a, int b, size_t c)
{
	return FMemory::Memset(a, b, c);
}

void* VPXMemmove(void* a, const void* b, size_t c)
{
	return FMemory::Memmove(a, b, c);
}

void FWebMRecord::EncodeVideoAndAudio(const FString& Filename)
{
	bCompressionSuccessful = false;

	// Pick a proper filename for the video
	FString BasePath = FPaths::ScreenShotDir();
	FString WebMPath = BasePath / TEXT("anim.webm");

	if (Filename.IsEmpty())
	{
		static int32 WebMIndex = 0;
		const int32 MaxTestWebMIndex = 65536;
		for (int32 TestWebMIndex = WebMIndex + 1; TestWebMIndex < MaxTestWebMIndex; ++TestWebMIndex)
		{
			const FString TestFileName = BasePath / FString::Printf(TEXT("UTSelfie%05i.webm"), TestWebMIndex);
			if (IFileManager::Get().FileSize(*TestFileName) < 0)
			{
				WebMIndex = TestWebMIndex;
				WebMPath = TestFileName;
				break;
			}
		}
	}
	else
	{
		WebMPath = Filename;
	}

	// Open the file for writing
	FILE* file = nullptr;
	fopen_s(&file, TCHAR_TO_ANSI(*WebMPath), "wb");
	if (!file)
	{
		return;
	}

	const uint64 SecondsToNanoSeconds = 1000000000LL;

	// We really should just go with MkvMuxer stuff right here and skip write_webm_file_header, write_webm_block

	VpxVideoInfo info = { 0 };
	vpx_codec_ctx_t      codec;
	vpx_codec_enc_cfg_t  cfg;
	vpx_codec_err_t      res;
	int                  flags = 0;

	bool bProcessedAllAudio = false;
	bool bAudioPacketWaitingOnVideo = false;
	const uint32 SAMPLES_TO_READ = 1024;
	const int32 SAMPLE_SIZE = (int32)sizeof(short);
	short ReadBuffer[SAMPLES_TO_READ * SAMPLE_SIZE * 2];
	uint64 AudioSampleRate = 44100LL;

	// Hook up to the unreal memory allocators
	vpx_mem_set_functions(&VPXMalloc, &VPXCalloc, &VPXRealloc, &VPXFree, &VPXMemcpy, &VPXMemset, &VPXMemmove);

#define vpx_interface (vpx_codec_vp8_cx())

	res = vpx_codec_enc_config_default(vpx_interface, &cfg, 0);
	if (res)
	{
		return;
	}
	UE_LOG(LogUTWebM, Display, TEXT("Compressing with %s"), ANSI_TO_TCHAR(vpx_codec_iface_name(vpx_interface)));

	cfg.rc_target_bitrate = VideoWidth * VideoHeight * cfg.rc_target_bitrate / cfg.g_w / cfg.g_h;
	cfg.g_w = VideoWidth;
	cfg.g_h = VideoHeight;
	cfg.g_timebase.den = VideoFrameRate;

#define VP8_FOURCC 0x30385056
	info.codec_fourcc = VP8_FOURCC;
	info.frame_width = VideoWidth;
	info.frame_height = VideoHeight;
	info.time_base.numerator = cfg.g_timebase.num;
	info.time_base.denominator = cfg.g_timebase.den;
	
	vpx_image_t raw;
	if (!vpx_img_alloc(&raw, VPX_IMG_FMT_I420, VideoWidth, VideoHeight, 1))
	{
		return;
	}

	if (vpx_codec_enc_init(&codec, vpx_interface, &cfg, 0))
	{
		return;
	}

	// Open up our cached frame captures for reading
	FString TempFramesSavePath = FPaths::GameSavedDir() / TEXT("frames.raw");
	VideoTempFile = IFileManager::Get().CreateFileReader(*TempFramesSavePath);
	int64 FileSize = IFileManager::Get().FileSize(*TempFramesSavePath);

	int32 TotalFrameCount = FileSize / (VideoHeight * VideoWidth * sizeof(FColor));
	if (bWriteYUVToTempFile)
	{
		TotalFrameCount = FileSize / (VideoWidth * VideoHeight + VideoWidth * VideoHeight / 4 + VideoWidth * VideoHeight / 4);
	}

	// Open up our cached wave file for reading
	FString WavePath = FPaths::GameSavedDir() / TEXT("audio.wav");
	MMIOINFO mi = { 0 };
	HMMIO hAudioFile = mmioOpenA(TCHAR_TO_ANSI(*WavePath), &mi, MMIO_READ);
	MMCKINFO ckData = { 0 };
	MMCKINFO ckRIFF = { 0 };
	ckRIFF.ckid = MAKEFOURCC('W', 'A', 'V', 'E');
	MMRESULT mmr = mmioDescend(hAudioFile, &ckRIFF, NULL, MMIO_FINDRIFF);
	if (mmr == MMSYSERR_NOERROR)
	{
		ckData.ckid = MAKEFOURCC('d', 'a', 't', 'a');
		mmr = mmioDescend(hAudioFile, &ckData, &ckRIFF, MMIO_FINDCHUNK);
		if (mmr != MMSYSERR_NOERROR)
		{
			UE_LOG(LogUTWebM, Warning, TEXT("Could not find data chunk"));
		}
	}

	if (VideoRecordStart > 0 && VideoRecordFirstFrame > 0)
	{
		// Read (VideoRecordFirstFrame - VideoRecordStart) * AudioSampleRate out of the buffer to make up for video recording lag
		int32 ExtraSamples = (VideoRecordFirstFrame - VideoRecordStart) * AudioSampleRate;
		mmioSeek(hAudioFile, ExtraSamples * 2 * SAMPLE_SIZE, SEEK_CUR);
		UE_LOG(LogUTWebM, Log, TEXT("Read %d samples off the audio stream to match up with video"), ExtraSamples);
	}

	{
		int32 ExtraFrameSamples = VideoFrameDelay * AudioSampleRate;
		mmioSeek(hAudioFile, ExtraFrameSamples * 2 * SAMPLE_SIZE, SEEK_CUR);
		UE_LOG(LogUTWebM, Log, TEXT("Read %f seconds (%d samples) off the audio stream to match up with video"), VideoFrameDelay, ExtraFrameSamples);
	}

	// Start configuring vorbis for audio compression
	vorbis_info	vi;
	vorbis_info_init(&vi);
	// Assuming 2 channel, 44.1khz. Also setting max audio quality, could probably step on it a bit.
	vorbis_encode_init_vbr(&vi, 2, AudioSampleRate, 1.0f);
	ogg_packet op;
	op.granulepos = 0;
	op.packet = nullptr;
	op.bytes = 0;

	// Tell them UT wrote this
	vorbis_comment vc;
	vorbis_comment_init(&vc);
	vorbis_comment_add_tag(&vc, "ENCODER", "UnrealTournament");

	// Set up the analysis state and auxiliary encoding storage
	vorbis_dsp_state vd;
	vorbis_block vb;
	vorbis_analysis_init(&vd, &vi);
	vorbis_block_init(&vd, &vb);
	
	// Generate the three header packets needed to configure a decoder later
	ogg_packet header;
	ogg_packet header_comm;
	ogg_packet header_code;
	vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);

	uint8* AudioHeader;
	uint64 AudioHeaderLength;
	MakeAudioPrivateData(header, header_comm, header_code, &AudioHeader, AudioHeaderLength);

	struct EbmlGlobal ebml;
	ebml.last_pts_ns = -1;
	ebml.writer = NULL;
	ebml.segment = NULL;
	ebml.stream = file;
	struct vpx_rational framerate = cfg.g_timebase;
	write_webm_file_header_audio(&ebml, &cfg, &framerate, STEREO_FORMAT_MONO, VP8_FOURCC, AudioHeader, AudioHeaderLength);
	delete [] AudioHeader;

	int32 frame_cnt = 0;
	uint64 VideoTimeStamp = 0;
	uint64 NextVideoTimeStamp = 0;

	if (VideoTempFile)
	{
		TArray<FColor> VideoFrameTemp;
		VideoFrameTemp.Empty(VideoWidth * VideoHeight);
		VideoFrameTemp.AddZeroed(VideoHeight * VideoWidth);

		while (!VideoTempFile->AtEnd() && !bCancelCompressing)
		{
			if (bWriteYUVToTempFile)
			{
				VideoTempFile->Serialize(raw.planes[VPX_PLANE_Y], VideoWidth * VideoHeight);
				VideoTempFile->Serialize(raw.planes[VPX_PLANE_U], VideoWidth * VideoHeight / 4);
				VideoTempFile->Serialize(raw.planes[VPX_PLANE_V], VideoWidth * VideoHeight / 4);
			}
			else
			{
				// Just wrote raw rgb here
				VideoTempFile->Serialize(VideoFrameTemp.GetData(), VideoWidth * VideoHeight * sizeof(FColor));

				// Use libyuv to convert from ARGB to YUV
				libyuv::ARGBToI420((const uint8*)VideoFrameTemp.GetData(), VideoWidth * 4,
					raw.planes[VPX_PLANE_Y], raw.stride[VPX_PLANE_Y],
					raw.planes[VPX_PLANE_U], raw.stride[VPX_PLANE_U],
					raw.planes[VPX_PLANE_V], raw.stride[VPX_PLANE_V], VideoWidth, VideoHeight);
			}

			vpx_codec_encode(&codec, &raw, frame_cnt, 1, flags, VPX_DL_GOOD_QUALITY);
			vpx_codec_iter_t iter = NULL;
			const vpx_codec_cx_pkt_t *pkt;
			while ((pkt = vpx_codec_get_cx_data(&codec, &iter)) != NULL)
			{
				switch (pkt->kind)
				{
				case VPX_CODEC_CX_FRAME_PKT:
					write_webm_block(&ebml, &cfg, pkt);
					break;
				default:
					break;
				}
			}

			CompressionCompletionPercent = (float)frame_cnt / (float)TotalFrameCount;

			frame_cnt++;
			VideoTimeStamp = ebml.last_pts_ns;
			NextVideoTimeStamp = VideoTimeStamp + 1000000000ll * cfg.g_timebase.num / cfg.g_timebase.den;

			uint64 opTimeStamp = op.granulepos * SecondsToNanoSeconds / AudioSampleRate;
			while (opTimeStamp < NextVideoTimeStamp)
			{
				if (bAudioPacketWaitingOnVideo && op.packet != nullptr && op.bytes > 0)
				{
					// Make sure we are increasing
					if (opTimeStamp == VideoTimeStamp)
					{
						opTimeStamp += 1;
					}

					write_webm_audio_block(&ebml, op.packet, op.bytes, opTimeStamp);
					
					bAudioPacketWaitingOnVideo = false;
				}

				if (bProcessedAllAudio)
				{
					break;
				}

				// Make new packets
				while (vorbis_analysis_blockout(&vd, &vb) == 1)
				{
					// analysis, assume we want to use bitrate management
					vorbis_analysis(&vb, NULL);
					vorbis_bitrate_addblock(&vb);

					while (vorbis_bitrate_flushpacket(&vd, &op))
					{
						opTimeStamp = op.granulepos * SecondsToNanoSeconds / AudioSampleRate;
						if (opTimeStamp < NextVideoTimeStamp)
						{
							// Make sure we are increasing
							if (opTimeStamp == VideoTimeStamp)
							{
								opTimeStamp += 1;
							}

							write_webm_audio_block(&ebml, op.packet, op.bytes, opTimeStamp);
						}
						else
						{
							bAudioPacketWaitingOnVideo = true;
						}
					}
					
					if (bAudioPacketWaitingOnVideo)
					{
						break;
					}
				}

				// We have more audio packets than video frames, just wait until there's more video frames
				if (bAudioPacketWaitingOnVideo)
				{
					break;
				}

				if (opTimeStamp < NextVideoTimeStamp && !bProcessedAllAudio)
				{
					int32 BytesRead = mmioRead(hAudioFile, (HPSTR)ReadBuffer, SAMPLES_TO_READ * 2 * SAMPLE_SIZE);
					if (BytesRead < 0)
					{
						UE_LOG(LogUTWebM, Warning, TEXT("Error reading from source wave file"));
					}
					else if (BytesRead == 0)
					{
						// Write the end of file
						vorbis_analysis_wrote(&vd, 0);
						bProcessedAllAudio = true;
					}
					else
					{
						// expose the buffer to submit data
						float **buffer = vorbis_analysis_buffer(&vd, SAMPLES_TO_READ);

						// Assuming stereo
						int32 i = 0;
						for (i = 0; i < BytesRead / (SAMPLE_SIZE * 2); i++)
						{
							buffer[0][i] = (ReadBuffer[i * 2]) / 32768.0f;
							buffer[1][i] = (ReadBuffer[i * 2 + 1]) / 32768.0f;
						}

						// tell the library how many samples we actually submitted
						vorbis_analysis_wrote(&vd, i);
					}
				}
			}
		}

		VideoTempFile->Close();
		mmioClose(hAudioFile, 0);
	}

	// write final frame, pretty sure this doesn't do anything though
	vpx_codec_encode(&codec, nullptr, frame_cnt, 1, flags, VPX_DL_GOOD_QUALITY);
	vpx_codec_iter_t iter = NULL;
	const vpx_codec_cx_pkt_t *pkt;
	pkt = vpx_codec_get_cx_data(&codec, &iter);
	while ((pkt = vpx_codec_get_cx_data(&codec, &iter)) != NULL)
	{
		switch (pkt->kind)
		{
		case VPX_CODEC_CX_FRAME_PKT:
			write_webm_block(&ebml, &cfg, pkt);
			break;
		default:
			break;
		}
	}

	write_webm_file_footer(&ebml);

	fclose(file);

	bCompressionSuccessful = true;
	UE_LOG(LogUTWebM, Display, TEXT("Selfie video complete! %s"), *WebMPath);

	vpx_img_free(&raw);

	// Heap corruption has been detected on my machine when running under the debugger
	// Have not caught it when running app verifier or any other serious bounds checking
	// Might be a good idea to try updating libvpx
	if (vpx_codec_destroy(&codec))
	{
		// failed to destroy
	}
}

void FWebMRecord::DebugUploadLastVideo(UWorld* InWorld)
{
	// Find the last good one
	FString BasePath = FPaths::ScreenShotDir();
	FString WebMPath = BasePath / TEXT("anim.webm");
	const int32 MaxTestWebMIndex = 65536;
	for (int32 TestWebMIndex = 1; TestWebMIndex < MaxTestWebMIndex; ++TestWebMIndex)
	{
		const FString TestFileName = BasePath / FString::Printf(TEXT("UTSelfie%05i.webm"), TestWebMIndex);
		if (IFileManager::Get().FileSize(*TestFileName) > 0)
		{
			WebMPath = TestFileName;
		}
		else
		{
			break;
		}
	}

#if !UE_SERVER
	UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(InWorld, 0));
	FirstPlayer->RecordedReplayFilename = WebMPath;
	FirstPlayer->UploadVideoToYoutube();
#endif
}

void FWebMRecord::DebugWriteAudioToOGG()
{
	// Encode audio separately for testing right now
	uint32 AudioDataBytesWriten = 0;
	FString WavePath = FPaths::GameSavedDir() / TEXT("audio.wav");
	MMIOINFO mi = { 0 };
	HMMIO hAudioFile = mmioOpenA(TCHAR_TO_ANSI(*WavePath), &mi, MMIO_READ);
	MMCKINFO ckData = { 0 };
	MMCKINFO ckRIFF = { 0 };
	ckRIFF.ckid = MAKEFOURCC('R', 'I', 'F', 'F');
	ckRIFF.fccType = MAKEFOURCC('W', 'A', 'V', 'E');
	MMRESULT mmr = mmioDescend(hAudioFile, &ckRIFF, NULL, MMIO_FINDRIFF);
	if (mmr == MMSYSERR_NOERROR)
	{
		ckData.ckid = MAKEFOURCC('d', 'a', 't', 'a');
		mmr = mmioDescend(hAudioFile, &ckData, &ckRIFF, MMIO_FINDCHUNK);
		if (mmr != MMSYSERR_NOERROR)
		{
			UE_LOG(LogUTWebM, Warning, TEXT("Could not find data chunk"));
		}
	}

	const uint32 SAMPLES_TO_READ = 1024;
	const int32 SAMPLE_SIZE = (int32)sizeof(short);
	short ReadBuffer[SAMPLES_TO_READ * SAMPLE_SIZE * 2];

	FString OGGPath = FPaths::GameSavedDir() / TEXT("test.ogg");
	FArchive* AudioTempFile = IFileManager::Get().CreateFileWriter(*OGGPath);

	vorbis_info	vi;
	vorbis_info_init(&vi);
	// Assuming 2 channel, 44.1khz. Also setting max audio quality, could probably step on it a bit.
	vorbis_encode_init_vbr(&vi, 2, 44100, 1.0f);

	// Tell them UT wrote this
	vorbis_comment vc;
	vorbis_comment_init(&vc);
	vorbis_comment_add_tag(&vc, "ENCODER", "UnrealTournament");

	// Set up the analysis state and auxiliary encoding storage
	vorbis_dsp_state vd;
	vorbis_block vb;
	vorbis_analysis_init(&vd, &vi);
	vorbis_block_init(&vd, &vb);

	// Set up our packet->stream encoder
	ogg_stream_state os;
	ogg_page og;
	ogg_packet op;
	ogg_stream_init(&os, 0);

	ogg_packet header;
	ogg_packet header_comm;
	ogg_packet header_code;

	// Generate the three header packets needed to configure a decoder later
	vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);

	// Put each of the header packets into the stream
	ogg_stream_packetin(&os, &header);
	ogg_stream_packetin(&os, &header_comm);
	ogg_stream_packetin(&os, &header_code);

	// Flush stream to file and make sure the actual audio data will start on a new page, as per spec
	int32 StreamFlushResult;
	do
	{
		StreamFlushResult = ogg_stream_flush(&os, &og);
		AudioTempFile->Serialize(og.header, og.header_len);
		AudioTempFile->Serialize(og.body, og.body_len);

		AudioDataBytesWriten += og.header_len;
		AudioDataBytesWriten += og.body_len;
	} while (StreamFlushResult != 0);

	bool bEndOfStream = false;

	while (!bEndOfStream)
	{
		int32 BytesRead = mmioRead(hAudioFile, (HPSTR)ReadBuffer, SAMPLES_TO_READ * 2 * SAMPLE_SIZE);
		if (BytesRead < 0)
		{
			UE_LOG(LogUTWebM, Warning, TEXT("Error reading from source wave file"));
		}
		else if (BytesRead == 0)
		{
			// Write the end of file
			vorbis_analysis_wrote(&vd, 0);
		}
		else
		{
			// expose the buffer to submit data
			float **buffer = vorbis_analysis_buffer(&vd, SAMPLES_TO_READ);

			// Assuming stereo
			int32 i = 0;
			for (i = 0; i < BytesRead / (SAMPLE_SIZE * 2); i++)
			{
				buffer[0][i] = (ReadBuffer[i * 2]) / 32768.0f;
				buffer[1][i] = (ReadBuffer[i * 2 + 1]) / 32768.0f;
			}

			// tell the library how many samples we actually submitted
			vorbis_analysis_wrote(&vd, i);
		}

		// vorbis does some data preanalysis, then divvies up blocks for more involved (potentially parallel) processing.
		while (vorbis_analysis_blockout(&vd, &vb) == 1)
		{
			// analysis, assume we want to use bitrate management
			vorbis_analysis(&vb, NULL);
			vorbis_bitrate_addblock(&vb);

			while (vorbis_bitrate_flushpacket(&vd, &op))
			{
				// weld the packet into the bitstream
				ogg_stream_packetin(&os, &op);

				// write out pages (if any)
				while (!bEndOfStream)
				{
					int PageOutResult = ogg_stream_pageout(&os, &og);
					if (PageOutResult == 0)
					{
						break;
					}
					AudioTempFile->Serialize(og.header, og.header_len);
					AudioTempFile->Serialize(og.body, og.body_len);
					AudioDataBytesWriten += og.header_len;
					AudioDataBytesWriten += og.body_len;

					// this could be set above, but for illustrative purposes, I do	it here (to show that vorbis does know where the stream ends)
					if (ogg_page_eos(&og))
					{
						bEndOfStream = true;
					}
				}
			}
		}
	}

	AudioTempFile->Close();
	UE_LOG(LogUTWebM, Log, TEXT("Wrote %s with %d bytes"), *OGGPath, AudioDataBytesWriten);

	// clean up and exit.  vorbis_info_clear() must be called last
	ogg_stream_clear(&os);
	vorbis_block_clear(&vb);
	vorbis_dsp_clear(&vd);
	vorbis_comment_clear(&vc);
	vorbis_info_clear(&vi);
	// ogg_page and ogg_packet structs always point to storage in libvorbis.  They're never freed or manipulated directly

	mmioClose(hAudioFile, 0);
}

FCaptureAudioWorker::FCaptureAudioWorker()
{
	bStopCapture = false;

	hAudioFile = 0;

	bCapturingAudio = false;
	MMDevice = nullptr;
	AudioClient = nullptr;
	AudioCaptureClient = nullptr;
	WFX = nullptr;
	AudioTotalFrames = 0;
	StartingAudioTime = 0;

	// This needs to be last or you get race conditions with ::Run()
	Thread = FRunnableThread::Create(this, TEXT("FCaptureAudioWorker"), 0, TPri_Highest);
}

FCaptureAudioWorker* FCaptureAudioWorker::Runnable = nullptr;

uint32 FCaptureAudioWorker::Run()
{
	CoInitialize(NULL);

	DWORD nTaskIndex = 0;
	HANDLE hTask = AvSetMmThreadCharacteristics(L"Capture", &nTaskIndex);

	InitAudioLoopback();
	OpenTempWaveFile();

	// create a periodic waitable timer
	HANDLE hWakeUp = CreateWaitableTimer(NULL, FALSE, NULL);
	// set the waitable timer
	LARGE_INTEGER liFirstFire;
	liFirstFire.QuadPart = -hnsDefaultDevicePeriod / 2; // negative means relative time
	LONG lTimeBetweenFires = (LONG)hnsDefaultDevicePeriod / 2 / (10 * 1000); // convert to milliseconds
	BOOL bOK = SetWaitableTimer(hWakeUp, &liFirstFire, lTimeBetweenFires, NULL, NULL, FALSE);


	HANDLE waitArray[1] = { hWakeUp };
	DWORD dwWaitResult;

	while (!bStopCapture && bCapturingAudio)
	{
		UINT32 nNextPacketSize;
		for (HRESULT hr = AudioCaptureClient->GetNextPacketSize(&nNextPacketSize); hr == S_OK && nNextPacketSize > 0; hr = AudioCaptureClient->GetNextPacketSize(&nNextPacketSize))
		{
			BYTE* Data;
			UINT32 NumFramesRead;
			DWORD Flags;
			UINT64 QPCPosition;
			HRESULT hrGetBuffer = AudioCaptureClient->GetBuffer(&Data, &NumFramesRead, &Flags, nullptr, &QPCPosition);
			if (hrGetBuffer == S_OK)
			{
				uint32 DataSizeInBytes = NumFramesRead * AudioBlockAlign;
				AudioTotalFrames += NumFramesRead;

				if (hAudioFile)
				{
					mmioWrite(hAudioFile, (const char*)Data, DataSizeInBytes);
				}

				HRESULT hrReleaseBuffer = AudioCaptureClient->ReleaseBuffer(NumFramesRead);
			}

			dwWaitResult = WaitForMultipleObjects(ARRAYSIZE(waitArray), waitArray, FALSE, INFINITE);
		}
	}

	CloseTempWaveFile();
	StopAudioLoopback();

	AvRevertMmThreadCharacteristics(hTask);

	CoUninitialize();

	return 0;
}


void FCaptureAudioWorker::InitAudioLoopback()
{
	// Based on http://blogs.msdn.com/b/matthew_van_eerde/archive/2014/11/05/draining-the-wasapi-capture-buffer-fully.aspx

	IMMDeviceEnumerator* DeviceEnumerator = nullptr;
	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&DeviceEnumerator);
	if (hr == S_OK)
	{
		hr = DeviceEnumerator->GetDefaultAudioEndpoint(EDataFlow::eRender, ERole::eConsole, &MMDevice);
		if (hr == S_OK && MMDevice != nullptr)
		{
			const IID IID_IAudioClient = __uuidof(IAudioClient);
			hr = MMDevice->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&AudioClient);
			if (hr == S_OK && AudioClient)
			{
				hr = AudioClient->GetDevicePeriod(&hnsDefaultDevicePeriod, NULL);

				// this must be free'd with CoTaskMemFreeLater
				hr = AudioClient->GetMixFormat(&WFX);
				if (hr == S_OK)
				{
					// May need to create a silent audio stream to work around an issue from 2008, hopefully not anymore
					// https://social.msdn.microsoft.com/Forums/windowsdesktop/en-US/c7ba0a04-46ce-43ff-ad15-ce8932c00171/loopback-recording-causes-digital-stuttering?forum=windowspro-audiodevelopment

					// Adjust the desired WFX for capture to 16 bit PCM, not sure if necessary, makes writing to wave file for testing nice though
					if (WFX->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
					{
						WAVEFORMATEXTENSIBLE* WFEX = (WAVEFORMATEXTENSIBLE*)WFX;
						if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, WFEX->SubFormat))
						{
							WFEX->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
							WFX->wBitsPerSample = 16;
							WFEX->Samples.wValidBitsPerSample = WFX->wBitsPerSample;
							WFX->nBlockAlign = WFX->nChannels * WFX->wBitsPerSample / 8;
							WFX->nAvgBytesPerSec = WFX->nBlockAlign * WFX->nSamplesPerSec;
						}
					}
					else if (WFX->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
					{
						WFX->wFormatTag = WAVE_FORMAT_PCM;
						WFX->wBitsPerSample = 16;
						WFX->nBlockAlign = WFX->nChannels * WFX->wBitsPerSample / 8;
						WFX->nAvgBytesPerSec = WFX->nBlockAlign * WFX->nSamplesPerSec;
					}

					AudioBlockAlign = WFX->nBlockAlign;

					AudioTotalFrames = 0;

					// nanoseconds, value taken from windows sample
					const int32 REFTIMES_PER_SEC = 10000000;

					// Audio capture
					REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
					hr = AudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, hnsRequestedDuration, 0, WFX, 0);
					if (hr == S_OK)
					{
						const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
						hr = AudioClient->GetService(IID_IAudioCaptureClient, (void**)&AudioCaptureClient);
						if (hr == S_OK && AudioCaptureClient)
						{
							hr = AudioClient->Start();
							if (hr == S_OK)
							{
								bCapturingAudio = true;
							}
						}
					}
				}
			}
		}
		DeviceEnumerator->Release();
		DeviceEnumerator = nullptr;
	}
}

void FCaptureAudioWorker::OpenTempWaveFile()
{
	// Write wave file for debug
	ckRIFF = { 0 };
	ckData = { 0 };

	ckRIFF.ckid = MAKEFOURCC('R', 'I', 'F', 'F');
	ckRIFF.fccType = MAKEFOURCC('W', 'A', 'V', 'E');

	FString BasePath = FPaths::GameSavedDir();
	FString WavePath = BasePath / TEXT("audio.wav");
	MMIOINFO mi = { 0 };
	hAudioFile = mmioOpenA(TCHAR_TO_ANSI(*WavePath), &mi, MMIO_WRITE | MMIO_CREATE);

	if (hAudioFile)
	{
		MMRESULT mmr = mmioCreateChunk(hAudioFile, &ckRIFF, MMIO_CREATERIFF);
		if (mmr == MMSYSERR_NOERROR)
		{
			// fmt chunk
			MMCKINFO chunk;
			chunk.ckid = MAKEFOURCC('f', 'm', 't', ' ');
			mmr = mmioCreateChunk(hAudioFile, &chunk, 0);
			if (mmr == MMSYSERR_NOERROR)
			{
				LONG BytesInWFX = sizeof(WAVEFORMATEX)+WFX->cbSize;
				if (mmioWrite(hAudioFile, (const char*)WFX, BytesInWFX) == BytesInWFX)
				{
					mmr = mmioAscend(hAudioFile, &chunk, 0);
					if (mmr == MMSYSERR_NOERROR)
					{
						// fact chunk
						chunk.ckid = MAKEFOURCC('f', 'a', 'c', 't');
						mmr = mmioCreateChunk(hAudioFile, &chunk, 0);
						if (mmr == MMSYSERR_NOERROR)
						{
							// have to reopen the fact chunk when finished to put in total audio frames
							DWORD frames = 0;
							if (mmioWrite(hAudioFile, (const char*)&frames, sizeof(frames)) == sizeof(frames))
							{
								mmr = mmioAscend(hAudioFile, &chunk, 0);
								if (mmr == MMSYSERR_NOERROR)
								{
									ckData.ckid = MAKEFOURCC('d', 'a', 't', 'a');
									mmr = mmioCreateChunk(hAudioFile, &ckData, 0);
									if (mmr == MMSYSERR_NOERROR)
									{
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

void FCaptureAudioWorker::CloseTempWaveFile()
{
	MMRESULT mmr;
	if (hAudioFile)
	{
		mmr = mmioAscend(hAudioFile, &ckData, 0);
		mmr = mmioAscend(hAudioFile, &ckRIFF, 0);
		mmr = mmioClose(hAudioFile, 0);
		hAudioFile = NULL;

		FString WavePath = FPaths::GameSavedDir() / TEXT("audio.wav");
		MMIOINFO mi = { 0 };
		hAudioFile = mmioOpenA(TCHAR_TO_ANSI(*WavePath), &mi, MMIO_READWRITE);

		if (hAudioFile)
		{
			ckRIFF = { 0 };
			ckRIFF.ckid = MAKEFOURCC('W', 'A', 'V', 'E');
			mmr = mmioDescend(hAudioFile, &ckRIFF, NULL, MMIO_FINDRIFF);
			if (mmr == MMSYSERR_NOERROR)
			{
				MMCKINFO ckFact = { 0 };
				ckFact.ckid = MAKEFOURCC('f', 'a', 'c', 't');
				mmr = mmioDescend(hAudioFile, &ckFact, &ckRIFF, MMIO_FINDCHUNK);
				if (mmr == MMSYSERR_NOERROR)
				{
					DWORD frames = AudioTotalFrames;
					if (mmioWrite(hAudioFile, (const char*)&frames, sizeof(frames)) == sizeof(frames))
					{
					}

					mmioAscend(hAudioFile, &ckFact, 0);
				}
			}

			mmr = mmioClose(hAudioFile, 0);
		}
	}
}

void FCaptureAudioWorker::StopAudioLoopback()
{
	if (WFX)
	{
		// Should be done with WFX now
		CoTaskMemFree(WFX);
		WFX = nullptr;
	}

	if (AudioClient)
	{
		AudioClient->Stop();
	}

	if (AudioCaptureClient)
	{
		AudioCaptureClient->Release();
		AudioCaptureClient = nullptr;
	}

	if (AudioClient)
	{
		AudioClient->Release();
		AudioClient = nullptr;
	}

	if (MMDevice)
	{
		MMDevice->Release();
		MMDevice = nullptr;
	}
}

void FWebMRecord::StartCompressing(const FString& Filename)
{
	bCompressing = true;
	bCompressionComplete = false;
	bCancelCompressing = false;
	CompressionCompletionPercent = 0;
	
	CompressWorker = FCompressVideoWorker::RunWorkerThread(this, Filename);

	//CompressWorker = FCreateAnimatedGIFWorker::RunWorkerThread(this, Filename);
}

void FWebMRecord::CancelCompressing()
{
	bCancelCompressing = true;
	if (CompressWorker)
	{
		CompressWorker->WaitForCompletion();
	}
	bCompressing = false;
}

bool FWebMRecord::IsRecording(uint32& TickRate)
{
	TickRate = VideoFrameRate;
	return bRecording;
}

FCompressVideoWorker* FCompressVideoWorker::Runnable = nullptr;

FCompressVideoWorker::FCompressVideoWorker(FWebMRecord* InWebMRecorder, const FString& InFilename)
{
	WebMRecorder = InWebMRecorder;
	Filename = InFilename;

	// This needs to be last or you get race conditions with ::Run()
	Thread = FRunnableThread::Create(this, TEXT("FCompressVideoWorker"), 0, TPri_Highest);
}

uint32 FCompressVideoWorker::Run()
{
	WebMRecorder->EncodeVideoAndAudio(Filename);
	WebMRecorder->bCompressionComplete = true;

	return 0;
}


FCreateAnimatedGIFWorker* FCreateAnimatedGIFWorker::Runnable = nullptr;

FCreateAnimatedGIFWorker::FCreateAnimatedGIFWorker(FWebMRecord* InWebMRecorder, const FString& InFilename)
{
	WebMRecorder = InWebMRecorder;
	Filename = InFilename;

	// This needs to be last or you get race conditions with ::Run()
	Thread = FRunnableThread::Create(this, TEXT("FCreateAnimatedGIFWorker"), 0, TPri_Highest);
}

uint32 FCreateAnimatedGIFWorker::Run()
{
	WebMRecorder->EncodeAnimatedGIF(Filename);
	WebMRecorder->bCompressionComplete = true;

	return 0;
}

void FWebMRecord::EncodeAnimatedGIF(const FString& Filename)
{
	FColor* RGBAFrame = new FColor[VideoWidth * VideoHeight];
	uint8* YPlane = new uint8[VideoWidth * VideoHeight];
	uint8* UPlane = new uint8[VideoWidth * VideoHeight / 4];
	uint8* VPlane = new uint8[VideoWidth * VideoHeight / 4];

	FString TempFramesSavePath = FPaths::GameSavedDir() / TEXT("frames.raw");
	VideoTempFile = IFileManager::Get().CreateFileReader(*TempFramesSavePath);
	int64 FileSize = IFileManager::Get().FileSize(*TempFramesSavePath);

	int32 TotalFrameCount = FileSize / (VideoHeight * VideoWidth * sizeof(FColor));
	if (bWriteYUVToTempFile)
	{
		TotalFrameCount = FileSize / (VideoWidth * VideoHeight + VideoWidth * VideoHeight / 4 + VideoWidth * VideoHeight / 4);
	}
	
	gdImagePtr im; 
	gdImagePtr prev = nullptr;

	im = gdImageCreateTrueColor(VideoWidth, VideoHeight);
	FILE* out = nullptr;
	fopen_s(&out, TCHAR_TO_ANSI(*(Filename + TEXT(".gif"))), "wb");

	if (bWriteYUVToTempFile)
	{
		VideoTempFile->Serialize(YPlane, VideoWidth * VideoHeight);
		VideoTempFile->Serialize(UPlane, VideoWidth * VideoHeight / 4);
		VideoTempFile->Serialize(VPlane, VideoWidth * VideoHeight / 4);

		// Use libyuv to convert from YUV to ARGB
		libyuv::I420ToARGB(YPlane, VideoWidth, UPlane, VideoWidth / 2, VPlane, VideoWidth / 2, (uint8*)RGBAFrame, VideoWidth * 4, VideoWidth, VideoHeight);
	}
	else
	{
		VideoTempFile->Serialize(RGBAFrame, VideoWidth * VideoHeight * 4);
	}

	int i = 0;
	for (int y = 0; y < VideoHeight; y++)
	for (int x = 0; x < VideoWidth; x++)
	{
		int Color = gdTrueColor(RGBAFrame[i].R, RGBAFrame[i].G, RGBAFrame[i].B);
		gdImageSetPixel(im, x, y, Color);
		i++;
	}	

	gdImageGifAnimBegin(im, out, 0, 0);
	
	for (int32 FrameCount = 0; FrameCount < TotalFrameCount; FrameCount++)
	{
		if (FrameCount != 0)
		{
			if (bWriteYUVToTempFile)
			{
				VideoTempFile->Serialize(YPlane, VideoWidth * VideoHeight);
				VideoTempFile->Serialize(UPlane, VideoWidth * VideoHeight / 4);
				VideoTempFile->Serialize(VPlane, VideoWidth * VideoHeight / 4);

				// Use libyuv to convert from YUV to ARGB
				libyuv::I420ToARGB(YPlane, VideoWidth, UPlane, VideoWidth / 2, VPlane, VideoWidth / 2, (uint8*)RGBAFrame, VideoWidth * 4, VideoWidth, VideoHeight);
			}
			else
			{
				VideoTempFile->Serialize(RGBAFrame, VideoWidth * VideoHeight * 4);
			}

			im = gdImageCreateTrueColor(VideoWidth, VideoHeight);
			i = 0;
			for (int y = 0; y < VideoHeight; y++)
			for (int x = 0; x < VideoWidth; x++)
			{
				int Color = gdTrueColor(RGBAFrame[i].R, RGBAFrame[i].G, RGBAFrame[i].B);
				gdImageSetPixel(im, x, y, Color);
				i++;
			}
		}

		gdImageGifAnimAdd(im, out, 1, 0, 0, 100 * VideoFrameDelay, 1, prev);

		if (prev)
		{
			gdImageDestroy(prev);
		}

		prev = im;

		CompressionCompletionPercent = (float)FrameCount / (float)TotalFrameCount;
	}

	gdImageDestroy(prev);
	gdImageGifAnimEnd(out);
	fclose(out);

	delete[] RGBAFrame;
	delete[] YPlane;
	delete[] UPlane;
	delete[] VPlane;
}

#include "HideWindowsPlatformTypes.h"