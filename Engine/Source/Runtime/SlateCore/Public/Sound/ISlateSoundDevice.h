// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

struct FSlateSound;

/** Interface that Slate uses to play sounds. */
class SLATECORE_API ISlateSoundDevice
{
public:
	/** Play the sound resource within the given Slate sound object */
	virtual void PlaySound(const FSlateSound& Sound, int32 UserIndex = 0) const = 0;

	/** @return The duration of the given sound resource */
	virtual float GetSoundDuration(const FSlateSound& Sound) const = 0;

	/** Virtual destructor: because it is an interface / pure virtual base class. */
	virtual ~ISlateSoundDevice(){}
};

/** Silent implementation of ISlateSoundDevice; it plays nothing. */
class SLATECORE_API FNullSlateSoundDevice : public ISlateSoundDevice
{
public:
	virtual void PlaySound(const FSlateSound&, int32) const override
	{
	}

	virtual float GetSoundDuration(const FSlateSound& Sound) const override
	{
		return 0.0f;
	}

	virtual ~FNullSlateSoundDevice(){}
};
