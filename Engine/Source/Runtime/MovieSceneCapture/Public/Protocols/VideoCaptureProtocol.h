// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AVIWriter.h"
#include "FrameGrabberProtocol.h"
#include "VideoCaptureProtocol.generated.h"

UCLASS(config=EditorPerProjectUserSettings, DisplayName="Video Encoding")
class MOVIESCENECAPTURE_API UVideoCaptureSettings : public UFrameGrabberProtocolSettings
{
public:
	UVideoCaptureSettings(const FObjectInitializer& Init) : UFrameGrabberProtocolSettings(Init), bUseCompression(true), CompressionQuality(75) {}

	GENERATED_BODY()

	UPROPERTY(config, EditAnywhere, Category=VideoSettings)
	bool bUseCompression;

	UPROPERTY(config, EditAnywhere, Category=VideoSettings, meta=(ClampMin=1, ClampMax=100, EditCondition=bUseCompression))
	float CompressionQuality;

	UPROPERTY(config, EditAnywhere, Category=VideoSettings, AdvancedDisplay)
	FString VideoCodec;
};

struct MOVIESCENECAPTURE_API FVideoCaptureProtocol : FFrameGrabberProtocol
{
	virtual bool Initialize(const FCaptureProtocolInitSettings& InSettings, const ICaptureProtocolHost& Host) override;
	virtual void Finalize() override;
	virtual FFramePayloadPtr GetFramePayload(const FFrameMetrics& FrameMetrics, const ICaptureProtocolHost& Host);
	virtual void ProcessFrame(FCapturedFrameData Frame);
	virtual bool CanWriteToFile(const TCHAR* InFilename, bool bOverwriteExisting) const override;

protected:

	void ConditionallyCreateWriter(const ICaptureProtocolHost& Host);
private:

	TOptional<FCaptureProtocolInitSettings> InitSettings;

	TArray<TUniquePtr<FAVIWriter>> AVIWriters;
};