// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Persona.h"
#include "GraphEditor.h"
#include "SNodePanel.h"
#include "SAnimTrackPanel.h"
#include "SCurveEditor.h"
#include "Animation/AnimSequenceBase.h"

DECLARE_DELEGATE_OneParam( FOnSelectionChanged, const FGraphPanelSelectionSet& )
DECLARE_DELEGATE( FOnUpdatePanel )
DECLARE_DELEGATE_RetVal( float, FOnGetScrubValue )

//////////////////////////////////////////////////////////////////////////
// SAnimCurvePanel

class SAnimCurvePanel: public SAnimTrackPanel
{
public:
	SLATE_BEGIN_ARGS( SAnimCurvePanel )
		: _Sequence()
		, _ViewInputMin()
		, _ViewInputMax()
		, _InputMin()
		, _InputMax()
		, _OnSetInputViewRange()
		, _OnGetScrubValue()
	{}

	/**
	 * Weak ptr to our Persona instance
	 */
	SLATE_ARGUMENT(TWeakPtr<FPersona>, Persona)
	/**
	 * AnimSequenceBase to be used for this panel
	 */
	SLATE_ARGUMENT( class UAnimSequenceBase*, Sequence)
	/**
	 * Right side of widget width (outside of curve)
	 */
	SLATE_ARGUMENT( float, WidgetWidth )
	/**
	 * Viewable Range control variables
	 */
	SLATE_ATTRIBUTE( float, ViewInputMin )
	SLATE_ATTRIBUTE( float, ViewInputMax )
	SLATE_ATTRIBUTE( float, InputMin )
	SLATE_ATTRIBUTE( float, InputMax )
	SLATE_EVENT( FOnSetInputViewRange, OnSetInputViewRange )
	/**
	 * Get current value
	 */
	SLATE_EVENT( FOnGetScrubValue, OnGetScrubValue )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	/**
	 * Set New Sequence
	 */
	void SetSequence(class UAnimSequenceBase *	InSequence);

	/**
	 * Create New Curve Track
	 */
	FReply AddButtonClicked();

	/**
	 * Add Track
	 */
	void CreateTrack(const FText& CommittedText, ETextCommit::Type CommitInfo);

	/**
	 * Add new variable curve
	 */
	void AddVariableCurve(USkeleton::AnimCurveUID CurveUid);


	/**
	 * Duplicate Track
	 */
	FReply DuplicateTrack(USkeleton::AnimCurveUID Uid);

	/**
	 * Delete Track
	 *
	 */
	void DeleteTrack(USkeleton::AnimCurveUID Uid);

	/**
	 * Build and display curve track context menu for all tracks.
	 *
	 */
	FReply OnContextMenu();

	/**
	 * Toggle all the curve modes of all the tracks
	 */
	void ToggleAllCurveModes(ECheckBoxState NewState, EAnimCurveFlags ModeToSet);

	/**
	 * Are all the curve modes of all the tracks set to the mode
	 */
	ECheckBoxState AreAllCurvesOfMode(EAnimCurveFlags ModeToTest) const;

	/**
	 * Visibility of the set-all-tracks button
	 */
	EVisibility IsSetAllTracksButtonVisible() const;

	/**
	 * Creates a context menu for the provided curve
	 */
	TSharedRef<SWidget> CreateCurveContextMenu(FFloatCurve* Curve) const;

	/**
	 * Sets the specified flag value to State for the provided curve
	 */
	void SetCurveFlag(FFloatCurve* CurveInterface, bool State, EAnimCurveFlags FlagToSet);

	/**
	* Update Panel
	* Used internally and by sequence editor to refresh the panel contents
	* @todo this has to be more efficient. Right now it refreshes the entire panel
	*/
	void UpdatePanel();

private:
	TWeakPtr<FPersona> WeakPersona;
	TSharedPtr<SSplitter> PanelSlot;
	class UAnimSequenceBase* Sequence;
	TAttribute<float> CurrentPosition;
	FOnGetScrubValue OnGetScrubValue;
	TArray<TWeakPtr<class SCurveEdTrack>> Tracks;

	/**
	 * This is to control visibility of the curves, so you can edit or not
	 * Get Widget that shows all curve list and edit
	 */
	TSharedRef<SWidget>		GenerateCurveList();
	/**
	 * Returns true if this curve is editable
	 */
	ECheckBoxState	IsCurveEditable(USkeleton::AnimCurveUID Uid) const;
	/**
	 * Toggle curve visibility
	 */
	void		ToggleEditability(ECheckBoxState NewType, USkeleton::AnimCurveUID Uid);
	/**
	 * Refresh Panel
	 */
	FReply		RefreshPanel();
	/**
	 *	Show All Curves
	 */
	FReply		ShowAll(bool bShow);

	void FillEntryMenu(FMenuBuilder& Builder, FText AddToolTip, FExecuteAction& AddDelegate, FText CreateToolTip, FExecuteAction& CreateDelegate);

	/**
	 * Fills a menu for creating and adding new metadata entries
	 */
	void FillMetadataEntryMenu(FMenuBuilder& Builder);

	/**
	 * Fills a menu for creating and adding new variable curves
	 */
	void FillVariableCurveMenu(FMenuBuilder& Builder);

	/**
	 * Adds a metadata entry from a Uid for a name that exists in the skeleton
	 */
	void AddMetadataEntry(USkeleton::AnimCurveUID Uid);

	/**
	 * Handler for the new metadata item in the context menu, spawns a text entry for the name
	 */
	void CreateNewMetadataEntryClicked();

	/**
	 * Create a new curve name in the skeleton using the committed text and add that curve to the sequence
	 */
	void CreateNewMetadataEntry(const FText& CommittedText, ETextCommit::Type CommitType);

	/**
	* Handler for the new curve item in the context menu, spawns a text entry for the name
	*/
	void CreateNewCurveClicked();

	/**
	 * Convert the requested flag bool value into a checkbox state
	 */
	ECheckBoxState GetCurveFlagAsCheckboxState(FFloatCurve* Curve, EAnimCurveFlags InFlag) const;

	/**
	 * Convert a given checkbox state into a flag value in the provided curve
	 */
	void SetCurveFlagFromCheckboxState(ECheckBoxState CheckState, FFloatCurve* Curve, EAnimCurveFlags InFlag);

	/**
	 * Handler for converting a curve from metadata to variable curve or vice versa
	 */
	void ToggleCurveTypeMenuCallback(FFloatCurve* Curve);
};