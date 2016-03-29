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

	/** Sets the upper bound of the in/out section */
	TSharedPtr< FUICommandInfo > SetInOutEnd;

	/** Sets the lower bound of the in/out section */
	TSharedPtr< FUICommandInfo > SetInOutStart;

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

	/** Toggles whether to lock the in/out to the start/end range on scrolling. */
	TSharedPtr< FUICommandInfo > ToggleLockInOutToStartEndRange;

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

	/** Toggles whether or not to lock the play rate to the frame rate. */
	TSharedPtr< FUICommandInfo > ToggleFixedTimeStepPlayback;

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

	/** Paste from the sequencer clipboard history */
	TSharedPtr< FUICommandInfo > PasteFromHistory;

	/** Convert the selected possessed objects to spawnables. These will be spawned and destroyed by sequencer as per object's the spawn track. */
	TSharedPtr< FUICommandInfo > ConvertToSpawnable;

	/** Discard all changes to the current movie scene. */
	TSharedPtr< FUICommandInfo > DiscardChanges;

	/** Attempts to fix broken actor references. */
	TSharedPtr< FUICommandInfo > FixActorReferences;


	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;
};
