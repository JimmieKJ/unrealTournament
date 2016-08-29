// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class FLevelSequenceEditorCommands
	: public TCommands<FLevelSequenceEditorCommands>
{
public:

	/** Default constructor. */
	FLevelSequenceEditorCommands();

	/** Initialize commands */
	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> CreateNewLevelSequenceInLevel;
	TSharedPtr<FUICommandInfo> CreateNewMasterSequenceInLevel;
	TSharedPtr<FUICommandInfo> ToggleCinematicViewportCommand;
};
