// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
	
	/** Toggle play */
	TSharedPtr< FUICommandInfo > TogglePlay;

	/** Play forward */
	TSharedPtr< FUICommandInfo > PlayForward;

	/** Rewind */
	TSharedPtr< FUICommandInfo > Rewind;

	/** Shuttle forward */
	TSharedPtr< FUICommandInfo > ShuttleForward;

	/** Shuttle backward */
	TSharedPtr< FUICommandInfo > ShuttleBackward;

	/** Pause */
	TSharedPtr< FUICommandInfo > Pause;

	/** Step forward */
	TSharedPtr< FUICommandInfo > StepForward;

	/** Step backward */
	TSharedPtr< FUICommandInfo > StepBackward;

	/** Step to next key */
	TSharedPtr< FUICommandInfo > StepToNextKey;

	/** Step to previous key */
	TSharedPtr< FUICommandInfo > StepToPreviousKey;

	/** Step to next camera key */
	TSharedPtr< FUICommandInfo > StepToNextCameraKey;

	/** Step to previous camera key */
	TSharedPtr< FUICommandInfo > StepToPreviousCameraKey;

	/** Set start playback range */
	TSharedPtr< FUICommandInfo > SetStartPlaybackRange;

	/** Set end playback range */
	TSharedPtr< FUICommandInfo > SetEndPlaybackRange;

	/** Reset the view range to the playback range */
	TSharedPtr< FUICommandInfo > ResetViewRange;

	/** Zoom into the view range */
	TSharedPtr< FUICommandInfo > ZoomInViewRange;

	/** Zoom out of the view range */
	TSharedPtr< FUICommandInfo > ZoomOutViewRange;

	/** Forces playback both in editor and at runtime to be evaluated at fixed frame intervals. */
	TSharedPtr< FUICommandInfo > ToggleForceFixedFrameIntervalPlayback;

	/** Toggle constraining the time cursor to the playback range */
	TSharedPtr< FUICommandInfo > ToggleKeepCursorInPlaybackRange;

	/** Toggle constraining the playback range to the section bounds */
	TSharedPtr< FUICommandInfo > ToggleKeepPlaybackRangeInSectionBounds;

	/** Expand all nodes and descendants */
	TSharedPtr< FUICommandInfo > ExpandAllNodesAndDescendants;

	/** Collapse all nodes and descendants */
	TSharedPtr< FUICommandInfo > CollapseAllNodesAndDescendants;

	/** Expand/collapse nodes */
	TSharedPtr< FUICommandInfo > ToggleExpandCollapseNodes;

	/** Expand/collapse nodes and descendants */
	TSharedPtr< FUICommandInfo > ToggleExpandCollapseNodesAndDescendants;

	/** Sets the upper bound of the selection range */
	TSharedPtr< FUICommandInfo > SetSelectionRangeEnd;

	/** Sets the lower bound of the selection range */
	TSharedPtr< FUICommandInfo > SetSelectionRangeStart;

	/** Clear and reset the selection range */
	TSharedPtr< FUICommandInfo > ResetSelectionRange;

	/** Select all keys that fall into the selection range*/
	TSharedPtr< FUICommandInfo > SelectKeysInSelectionRange;

	/** Sets a key at the current time for the selected actor */
	TSharedPtr< FUICommandInfo > SetKey;

	/** Sets the interp tangent mode for the selected keys to auto */
	TSharedPtr< FUICommandInfo > SetInterpolationCubicAuto;

	/** Sets the interp tangent mode for the selected keys to user */
	TSharedPtr< FUICommandInfo > SetInterpolationCubicUser;

	/** Sets the interp tangent mode for the selected keys to break */
	TSharedPtr< FUICommandInfo > SetInterpolationCubicBreak;

	/** Sets the interp tangent mode for the selected keys to linear */
	TSharedPtr< FUICommandInfo > SetInterpolationLinear;

	/** Sets the interp tangent mode for the selected keys to constant */
	TSharedPtr< FUICommandInfo > SetInterpolationConstant;

	/** Trim section to the left, keeping the right portion */
	TSharedPtr< FUICommandInfo > TrimSectionLeft;

	/** Trim section to the right, keeping the left portion */
	TSharedPtr< FUICommandInfo > TrimSectionRight;

	/** Split section */
	TSharedPtr< FUICommandInfo > SplitSection;

	/** Set the auto key mode to Key All. */
	TSharedPtr< FUICommandInfo > SetAutoKeyModeAll;

	/** Set the auto key mode to Key Animated. */
	TSharedPtr< FUICommandInfo > SetAutoKeyModeAnimated;

	/** Set the auto key mode to Key None. */
	TSharedPtr< FUICommandInfo > SetAutoKeyModeNone;

	/** Turns key all on and off. */
	TSharedPtr< FUICommandInfo > ToggleKeyAllEnabled;

	/** Turns show frame numbers on and off. */
	TSharedPtr< FUICommandInfo > ToggleShowFrameNumbers;

	/** Toggle the visibility of the goto box. */
	TSharedPtr< FUICommandInfo > ToggleShowGotoBox;

	/** Turns the range slider on and off. */
	TSharedPtr< FUICommandInfo > ToggleShowRangeSlider;

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

	/** Toggles whether or not snap to key times while scrubbing. */
	TSharedPtr< FUICommandInfo > ToggleSnapPlayTimeToKeys;

	/** Toggles whether or not the play time should snap to the selected interval. */
	TSharedPtr< FUICommandInfo > ToggleSnapPlayTimeToInterval;

	/** Toggles whether or not the play time should snap to the dragged key. */
	TSharedPtr< FUICommandInfo > ToggleSnapPlayTimeToDraggedKey;

	/** Toggles whether or not to snap curve values to the interval. */
	TSharedPtr< FUICommandInfo > ToggleSnapCurveValueToInterval;

	/** Finds the viewed sequence asset in the content browser. */
	TSharedPtr< FUICommandInfo > FindInContentBrowser;

	/** Toggles whether to show combined keys at the top node level. */
	TSharedPtr< FUICommandInfo > ToggleCombinedKeyframes;

	/** Toggles whether to show channel colors in the track area. */
	TSharedPtr< FUICommandInfo > ToggleChannelColors;

	/** Toggles whether the label browser is enabled in the level editor. */
	TSharedPtr< FUICommandInfo > ToggleLabelBrowser;

	/** Turns auto scroll on and off. */
	TSharedPtr< FUICommandInfo > ToggleAutoScroll;

	/** Toggles whether or not the curve editor should be shown. */
	TSharedPtr< FUICommandInfo > ToggleShowCurveEditor;

	/** Toggles whether or not the curve editor time range should be linked to the sequencer. */
	TSharedPtr< FUICommandInfo > ToggleLinkCurveEditorTimeRange;

	/** Enable the move tool */
	TSharedPtr< FUICommandInfo > MoveTool;

	/** Enable the marquee selection tool */
	TSharedPtr< FUICommandInfo > MarqueeTool;

	/** Open a panel that enables exporting the sequence to a movie */
	TSharedPtr< FUICommandInfo > RenderMovie;

	/** Create camera and set it as the current camera cut */
	TSharedPtr< FUICommandInfo > CreateCamera;

	/** Paste from the sequencer clipboard history */
	TSharedPtr< FUICommandInfo > PasteFromHistory;

	/** Convert the selected possessed objects to spawnables. These will be spawned and destroyed by sequencer as per object's the spawn track. */
	TSharedPtr< FUICommandInfo > ConvertToSpawnable;

	/** Convert the selected spawnable objects to possessables. The newly created possessables will be created in the current level. */
	TSharedPtr< FUICommandInfo > ConvertToPossessable;

	/** Discard all changes to the current movie scene. */
	TSharedPtr< FUICommandInfo > DiscardChanges;

	/** Attempts to fix broken actor references. */
	TSharedPtr< FUICommandInfo > FixActorReferences;

	/** Attempts to move all time data for this sequence on to a valid frame */
	TSharedPtr< FUICommandInfo > FixFrameTiming;

	/** Record the selected actors into a sub sequence of the currently active sequence */
	TSharedPtr< FUICommandInfo > RecordSelectedActors;

	/** Imports animation from fbx. */
	TSharedPtr< FUICommandInfo > ImportFBX;

	/** Exports animation to fbx. */
	TSharedPtr< FUICommandInfo > ExportFBX;

	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;
};
