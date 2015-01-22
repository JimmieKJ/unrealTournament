// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace EPlaybackMode
{
	enum Type
	{
		Stopped,
		PlayingForward,
		PlayingReverse,
		Recording,
	};
}

DECLARE_DELEGATE_RetVal(bool, FOnGetLooping)
DECLARE_DELEGATE_RetVal(EPlaybackMode::Type, FOnGetPlaybackMode)

struct FTransportControlArgs
{
	FTransportControlArgs()
		: OnForwardPlay()
		, OnRecord()
		, OnBackwardPlay()
		, OnForwardStep()
		, OnBackwardStep()
		, OnForwardEnd()
		, OnBackwardEnd()
		, OnToggleLooping()
		, OnGetLooping()
		, OnGetPlaybackMode()
	{}

	FOnClicked OnForwardPlay;
	FOnClicked OnRecord;
	FOnClicked OnBackwardPlay;
	FOnClicked OnForwardStep;
	FOnClicked OnBackwardStep;
	FOnClicked OnForwardEnd;
	FOnClicked OnBackwardEnd;
	FOnClicked OnToggleLooping;
	FOnGetLooping OnGetLooping;
	FOnGetPlaybackMode OnGetPlaybackMode;
};

/**
 * Base class for a widget that allows transport control
 */
class ITransportControl : public SCompoundWidget
{
public:
};
