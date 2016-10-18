// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Sound/SoundWave.h"
#include "HapticFeedbackEffect_SoundWave.generated.h"

UCLASS(MinimalAPI, BlueprintType)
class UHapticFeedbackEffect_SoundWave : public UHapticFeedbackEffect_Base
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = "HapticFeedbackEffect_SoundWave")
	USoundWave *SoundWave;

	~UHapticFeedbackEffect_SoundWave();

	void Initialize() override;

	void GetValues(const float EvalTime, FHapticFeedbackValues& Values) override;

	float GetDuration() const override;

private:
	void PrepareSoundWaveBuffer();
	bool bPrepared;

	FHapticFeedbackBuffer HapticBuffer;
};
