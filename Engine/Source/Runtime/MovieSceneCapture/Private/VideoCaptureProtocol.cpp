// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCapturePCH.h"
#include "VideoCaptureProtocol.h"
#include "MovieSceneCapture.h"

bool FVideoCaptureProtocol::Initialize(const FCaptureProtocolInitSettings& InSettings, const ICaptureProtocolHost& Host)
{
	if (!FFrameGrabberProtocol::Initialize(InSettings, Host))
	{
		return false;
	}

	UVideoCaptureSettings* CaptureSettings = CastChecked<UVideoCaptureSettings>(InSettings.ProtocolSettings);

#if PLATFORM_MAC
	static const TCHAR* Extension = TEXT(".mov");
#else
	static const TCHAR* Extension = TEXT(".avi");
#endif

	FAVIWriterOptions Options;
	Options.OutputFilename = Host.GenerateFilename(FFrameMetrics(), Extension);
	Options.CaptureFPS = Host.GetCaptureFrequency();
	Options.CodecName = CaptureSettings->VideoCodec;
	Options.bSynchronizeFrames = Host.GetCaptureStrategy().ShouldSynchronizeFrames();
	Options.Width = InSettings.DesiredSize.X;
	Options.Height = InSettings.DesiredSize.Y;

	if (CaptureSettings->bUseCompression)
	{
		Options.CompressionQuality = CaptureSettings->CompressionQuality / 100.f;
		
		float QualityOverride = 100.f;
		if (FParse::Value( FCommandLine::Get(), TEXT( "-MovieQuality=" ), QualityOverride ))
		{
			Options.CompressionQuality = FMath::Clamp(QualityOverride, 1.f, 100.f) / 100.f;
		}

		Options.CompressionQuality = FMath::Clamp<float>(Options.CompressionQuality.GetValue(), 0.f, 1.f);
	}

	AVIWriter.Reset(FAVIWriter::CreateInstance(Options));
	AVIWriter->Initialize();

	return AVIWriter->IsCapturing();
}

struct FVideoFrameData : IFramePayload
{
	FFrameMetrics Metrics;
};

FFramePayloadPtr FVideoCaptureProtocol::GetFramePayload(const FFrameMetrics& FrameMetrics, const ICaptureProtocolHost& Host) const
{
	TSharedRef<FVideoFrameData, ESPMode::ThreadSafe> FrameData = MakeShareable(new FVideoFrameData);
	FrameData->Metrics = FrameMetrics;
	return FrameData;
}

void FVideoCaptureProtocol::ProcessFrame(FCapturedFrameData Frame)
{
	FVideoFrameData* Payload = Frame.GetPayload<FVideoFrameData>();

	AVIWriter->DropFrames(Payload->Metrics.NumDroppedFrames);
	AVIWriter->Update(Payload->Metrics.TotalElapsedTime, MoveTemp(Frame.ColorBuffer));
}

void FVideoCaptureProtocol::Finalize()
{
	AVIWriter->Finalize();
	AVIWriter.Reset();

	FFrameGrabberProtocol::Finalize();
}