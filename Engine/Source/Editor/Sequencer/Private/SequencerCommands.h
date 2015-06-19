// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FSequencerCommands : public TCommands<FSequencerCommands>
{

public:
	FSequencerCommands() : TCommands<FSequencerCommands>
	(
		"Sequencer",
		NSLOCTEXT("Contexts", "Sequencer", "Sequencer"),
		NAME_None, // "MainFrame" // @todo Fix this crash
		FEditorStyle::GetStyleSetName() // Icon Style Set
	)
	{}
	
	/** Sets a key at the current time for the selected actor */
	TSharedPtr< FUICommandInfo > SetKey;

	/** Turns auto keying on and off. */
	TSharedPtr< FUICommandInfo > ToggleAutoKeyEnabled;

	/** Turns snapping on and off. */
	TSharedPtr< FUICommandInfo > ToggleIsSnapEnabled;

	/** Toggles whether or not keys should snap to the selected interval. */
	TSharedPtr< FUICommandInfo > ToggleSnapKeyTimesToInterval;

	/** Toggles whether or not keys should snap to other keys in the section. */
	TSharedPtr< FUICommandInfo > ToggleSnapKeyTimesToKeys;

	/** Toggles whether or not sections should snap to the selected interval. */
	TSharedPtr< FUICommandInfo > ToggleSnapSectionTimesToInterval;

	/** Toggles whether or not sections should snap to other sections. */
	TSharedPtr< FUICommandInfo > ToggleSnapSectionTimesToSections;

	/** Toggles whether or not the play time should snap to the selected interval. */
	TSharedPtr< FUICommandInfo > ToggleSnapPlayTimeToInterval;

	/** Toggles whether or not to snap curve values to the interval. */
	TSharedPtr< FUICommandInfo > ToggleSnapCurveValueToInterval;

	/** Toggles whether the "clean view" is enabled in the level editor. */
	TSharedPtr< FUICommandInfo > ToggleCleanView;

	/** Toggles whether or not the curve editor should be shown. */
	TSharedPtr< FUICommandInfo > ToggleShowCurveEditor;

	/** Toggles whether or not to show tool tips for the curves in the curve editor. */
	TSharedPtr< FUICommandInfo > ToggleShowCurveEditorCurveToolTips;

	/** Sets the curve visibility to all curves. */
	TSharedPtr< FUICommandInfo > SetAllCurveVisibility;

	/** Sets the curve visibility to the selected curves. */
	TSharedPtr< FUICommandInfo > SetSelectedCurveVisibility;

	/** Sets the curve visibility to the animated curves. */
	TSharedPtr< FUICommandInfo > SetAnimatedCurveVisibility;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;
};
