// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Runtime/Core/Public/Features/IModularFeature.h"

class UTVideoRecordingFeature : public IModularFeature
{
public:
	virtual void StartRecording(float RecordTime) = 0;
	virtual void CancelRecording() = 0;
	
	DECLARE_MULTICAST_DELEGATE(FRecordingComplete);
	virtual FRecordingComplete& OnRecordingComplete() = 0;

	DECLARE_MULTICAST_DELEGATE_OneParam(FCompressingComplete, bool);
	virtual FCompressingComplete& OnCompressingComplete() = 0;

	virtual void StartCompressing(const FString& Filename) = 0;
	virtual float GetCompressionCompletionPercent() = 0;
	virtual void CancelCompressing() = 0;

	virtual bool IsRecording(uint32& TickRate) = 0;
};