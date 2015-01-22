// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Interface for audio format modules.
 */
class IAudioFormatModule
	: public IModuleInterface
{
public:

	/**
	 * Gets the audio format.
	 *
	 * @return The audio format interface.
	 */
	virtual IAudioFormat* GetAudioFormat() = 0;

public:

	/** Virtual destructor. */
	~IAudioFormatModule() { }
};
