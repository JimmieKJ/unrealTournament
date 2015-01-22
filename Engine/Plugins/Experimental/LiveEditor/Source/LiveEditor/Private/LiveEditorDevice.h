// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "portmidi.h"

struct FLiveEditorDeviceData
{
	enum EConfigState
	{
		UNCONFIGURED,
		CONFIGURED
	};

	FLiveEditorDeviceData(): ConfigState(UNCONFIGURED), LastInputTime(0.f) {}

	FString DeviceName;
	EConfigState ConfigState;

	int ButtonSignalDown;		//needs to remain int (instead of int32) since numbers are derived from TPL that uses int
	int ButtonSignalUp;			//needs to remain int (instead of int32) since numbers are derived from TPL that uses int

	int ContinuousIncrement;	//needs to remain int (instead of int32) since numbers are derived from TPL that uses int
	int ContinuousDecrement;	//needs to remain int (instead of int32) since numbers are derived from TPL that uses int

	float LastInputTime;
};

struct FLiveEditorDeviceConnection
{
	PortMidiStream *MIDIStream;
};

struct FLiveEditorDeviceInstance
{
	FLiveEditorDeviceData Data;
	FLiveEditorDeviceConnection Connection;
};
