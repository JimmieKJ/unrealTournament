// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands.h"

class FSequenceRecorderCommands : public TCommands<FSequenceRecorderCommands>
{
public:
	FSequenceRecorderCommands();

	TSharedPtr<FUICommandInfo> RecordAll;
	TSharedPtr<FUICommandInfo> StopAll;
	TSharedPtr<FUICommandInfo> AddRecording;
	TSharedPtr<FUICommandInfo> RemoveRecording;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};
