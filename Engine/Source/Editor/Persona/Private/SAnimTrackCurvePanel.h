// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Persona.h"
#include "GraphEditor.h"
#include "SNodePanel.h"
#include "SAnimTrackPanel.h"
#include "SCurveEditor.h"
#include "SAnimNotifyPanel.h"
#include "Animation/AnimSequenceBase.h"

//////////////////////////////////////////////////////////////////////////
// SAnimTrackCurvePanel

class SAnimTrackCurvePanel: public SAnimTrackPanel
{
public:
	SLATE_BEGIN_ARGS( SAnimTrackCurvePanel )
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
	SLATE_ARGUMENT( class UAnimSequence*, Sequence)
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
	 * Delete Track
	 *
	 */
	void DeleteTrack(USkeleton::AnimCurveUID Uid);

	/**
	 * Sets the specified flag value to State for the provided curve
	 */
	void SetCurveFlag(FAnimCurveBase* CurveInterface, bool State, EAnimCurveFlags FlagToSet);

	/**
	* Update Panel
	* Used internally and by sequence editor to refresh the panel contents
	* @todo this has to be more efficient. Right now it refreshes the entire panel
	*/
	void UpdatePanel();

	/** Get Context Menu Per Track **/
	TSharedRef<SWidget> CreateCurveContextMenu(USkeleton::AnimCurveUID CurveUid) const;

	/** Get Length of Sequence */
	float GetLength() { return (Sequence)? Sequence->SequenceLength : 0.f; }

private:
	TWeakPtr<FPersona> WeakPersona;
	TSharedPtr<SSplitter> PanelSlot;
	class UAnimSequence* Sequence;
	TAttribute<float> CurrentPosition;
	FOnGetScrubValue OnGetScrubValue;
	TArray<TWeakPtr<class STransformCurveEdTrack>> Tracks;

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

	/**
	 * Convert the requested flag bool value into a checkbox state
	 */
	ECheckBoxState GetCurveFlagAsCheckboxState(USkeleton::AnimCurveUID CurveUid, EAnimCurveFlags InFlag) const;

	/**
	 * Convert a given checkbox state into a flag value in the provided curve
	 */
	void SetCurveFlagFromCheckboxState(ECheckBoxState CheckState, USkeleton::AnimCurveUID CurveUid, EAnimCurveFlags InFlag);
};