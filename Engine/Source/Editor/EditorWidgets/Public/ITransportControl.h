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
DECLARE_DELEGATE_TwoParams(FOnTickPlayback, double /*InCurrentTime*/, float /*InDeltaTime*/)

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
		, OnTickPlayback()
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
	FOnTickPlayback OnTickPlayback;
};

/**
 * Base class for a widget that allows transport control
 */
class ITransportControl : public SCompoundWidget
{
public:
};
