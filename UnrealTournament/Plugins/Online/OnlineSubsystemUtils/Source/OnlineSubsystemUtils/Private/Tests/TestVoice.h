// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Ticker.h"

#if WITH_DEV_AUTOMATION_TESTS

class FTestVoice : public FTickerObjectBase, public FSelfRegisteringExec
{
public:

	FTestVoice();
	virtual ~FTestVoice();

	// FTickerObjectBase
	virtual bool Tick(float DeltaTime) override;

	// FSelfRegisteringExec
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	void Test();

private:

	class UAudioComponent* VoiceComp;
	TSharedPtr<class IVoiceCapture> VoiceCapture;
	TSharedPtr<class IVoiceEncoder> VoiceEncoder;
	TSharedPtr<class IVoiceDecoder> VoiceDecoder;

	TArray<uint8> RawCaptureData;
	int32 MaxRawCaptureDataSize;
	TArray<uint8> CompressedData;
	int32 MaxCompressedDataSize;
	TArray<uint8> UncompressedData;
	int32 MaxUncompressedDataSize;
	TArray<uint8> Remainder;
	int32 MaxRemainderSize;
	int32 LastRemainderSize;

	bool bUseTestSample;
	bool bZeroInput;
	bool bUseDecompressed;
	bool bZeroOutput;

	bool Init();
	void Shutdown();

	void SetStaticVoiceData(TArray<uint8>& VoiceData, uint32& TotalVoiceBytes);
};

#endif //WITH_DEV_AUTOMATION_TESTS