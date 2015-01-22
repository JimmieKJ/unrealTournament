// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "EnginePrivate.h"
#include "Sound/ISlateSoundDevice.h"

class FSlateSoundDevice : public ISlateSoundDevice
{
public:
	virtual ~FSlateSoundDevice(){}

private:
	virtual void PlaySound(const FSlateSound& Sound, int32 UserIndex = 0) const override;	
	virtual float GetSoundDuration(const FSlateSound& Sound) const override;
};
