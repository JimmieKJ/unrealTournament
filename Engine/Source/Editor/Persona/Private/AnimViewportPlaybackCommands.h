// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Class containing commands for viewport playback actions
 */
class FAnimViewportPlaybackCommands : public TCommands<FAnimViewportPlaybackCommands>
{
public:
	FAnimViewportPlaybackCommands();

	/** Command list, indexed by EPlaybackSpeeds*/
	TArray<TSharedPtr< FUICommandInfo >> PlaybackSpeedCommands;

public:
	/** Registers our commands with the binding system */
	virtual void RegisterCommands() override;
};
