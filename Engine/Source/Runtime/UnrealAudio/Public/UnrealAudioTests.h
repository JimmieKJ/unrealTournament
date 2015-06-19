// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "UnrealAudioModule.h"

namespace UAudio
{
	/**
	* TestDeviceQuery
	*
	* Test which queries input and output devices and displays information to log about device capabilities, format, etc.
	* @return True if test succeeds.
	*/
	bool UNREALAUDIO_API TestDeviceQuery();

	/**
	* TestDeviceOutputSimple
	*
	* Test which runs a simple sinusoid output on all channels (each channel is one octave higher than left channel)
	* to test that audio output is working and function with no discontinuities.
	* @param TestTime The amount of time to run the test in seconds (negative value means to run indefinitely)
	* @return True if test succeeds.
	*/
	bool UNREALAUDIO_API TestDeviceOutputSimple(double TestTime);

	/**
	* TestDeviceOutputRandomizedFm
	*
	* Test which tests output device functionality with a bit more fun than the simple test. You should hear randomized FM synthesis in panning through
	* all connected speaker outputs.
	* @param TestTime The amount of time to run the test in seconds (negative value means to run indefinitely)
	* @return True if test succeeds.
	*/
	bool UNREALAUDIO_API TestDeviceOutputRandomizedFm(double TestTime);

	/**
	* TestDeviceOutputNoisePan
	*
	* Test which tests output device functionality with white noise that pans clockwise in surround sound setup.
	* @param TestTime The amount of time to run the test in seconds (negative value means to run indefinitely)
	* @return True if test succeeds.
	*/
	bool UNREALAUDIO_API TestDeviceOutputNoisePan(double TestTime);

}

