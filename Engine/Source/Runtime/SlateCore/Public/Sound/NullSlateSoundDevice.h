// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/ISlateSoundDevice.h"

struct FSlateSound;

/** Silent implementation of ISlateSoundDevice; it plays nothing. */
class SLATECORE_API FNullSlateSoundDevice : public ISlateSoundDevice
{
public:
	virtual void PlaySound(const FSlateSound&, int32) const override;
	virtual float GetSoundDuration(const FSlateSound& Sound) const override;
	virtual ~FNullSlateSoundDevice();
};
