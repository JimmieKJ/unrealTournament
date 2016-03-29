// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
* Playable sound object for wave files that are generated from a Sound Mod
*/

#pragma once
#include "xmp.h"
#include "Sound/SoundWaveProcedural.h"
#include "SoundModWave.generated.h"

UCLASS()
class USoundModWave : public USoundWaveProcedural
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	class USoundMod* SoundMod;

	xmp_context xmpContext;

	//~ Begin USoundWave Interface.
	virtual int32 GeneratePCMData(uint8* PCMData, const int32 SamplesNeeded) override;
	//~ End USoundWave Interface.

};