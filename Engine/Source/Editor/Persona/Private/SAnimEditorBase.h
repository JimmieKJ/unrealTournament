// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Persona.h"
#include "GraphEditor.h"
#include "SNodePanel.h"
#include "SAnimCurvePanel.h"
#include "EditorObjectsTracker.h"

//////////////////////////////////////////////////////////////////////////
// FTrackMarkerBar

// Simple collection class to store data about a track marker (section, branch point etc)
class FTrackMarkerBar
{
public:
	float Time;
	FLinearColor DrawColour;
};

//////////////////////////////////////////////////////////////////////////
// SAnimEditorBase

/** Common functionality for all anim editor widgets */
class SAnimEditorBase : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS( SAnimEditorBase )
		: _Persona()
		, _DisplayAnimInfoBar(true)
		{}

		SLATE_ARGUMENT( TSharedPtr<FPersona>, Persona )

		SLATE_ARGUMENT( bool, DisplayAnimInfoBar )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Accessors to the current viewed Min/Max input range of the editor */
	float GetViewMinInput() const { return ViewMinInput; }
	float GetViewMaxInput() const { return ViewMaxInput; }

	/** Accessors to the total Min/Max input range of the current object being edited */
	float GetMinInput() const		{ return 0.f; }
	float GetMaxInput() const		{ return GetSequenceLength(); }

	void SetInputViewRange(float InViewMinInput, float InViewMaxInput);

	/** Scrolls the input view based on the supplied ScreenDelta */
	void ScrollInputViewRange(float ScreenDelta, FVector2D ScreenViewSize);

	/** Delegate when selection changed **/
	void OnSelectionChanged(const FGraphPanelSelectionSet& SelectedItems);

	/** Get Current Scrub Value **/
	float GetScrubValue() const;

	/** Creates an editor object from the given type to be used in a details panel */
	UObject* ShowInDetailsView( UClass* EdClass );

	/** Clears the detail view of whatever we displayed last */
	void ClearDetailsView();

	// FGCObject interface start
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	// FGCObject interface end

	/** Creates a anchor widget for each animation editor type */
	virtual TSharedRef<SWidget> CreateDocumentAnchor();

protected:
	/** Persona reference **/
	TWeakPtr<FPersona> PersonaPtr;

	/** Allows derived classes to create different animation scrub panel */
	virtual TSharedRef<class SAnimationScrubPanel> ConstructAnimScrubPanel();

	/** Allows derived classes to init newly created editor objects */
	virtual void InitDetailsViewEditorObject(class UEditorAnimBaseObj* EdObj) {};

	/** Get the length of the current sequence */
	float GetSequenceLength() const;

	/** Get the sequence that is currently being edited */
	virtual UAnimationAsset* GetEditorObject() const PURE_VIRTUAL(SAnimEditorBase::GetEditorObject, return NULL;);

	/** Get Name of Object being edited **/
	FText GetEditorObjectName() const;

	/** Get Min/Max Input of value **/
	float ViewMinInput;
	float ViewMaxInput;

	/** To get scrub value, get preview instance **/
	class UAnimSingleNodeInstance* GetPreviewInstance() const;

	/** For the information section **/
	FText GetCurrentSequenceTime() const;
	FText GetCurrentPercentage() const;
	FText GetCurrentFrame() const;

	/** Recalculate sequence length after modifying */
	void RecalculateSequenceLength();

	/** Clamps the sequence to the specified length 
	 *
	 *  @return		Whether clamping was/is necessary
	 */
	virtual bool ClampToEndTime(float NewEndTime);

	/** Calculates the sequence length of the object 
	 *
	 *  @return		New sequence length
	 */
	virtual float CalculateSequenceLengthOfEditorObject() const {return GetSequenceLength();}

	/** The slate container that the editor panels are placed in */
	TSharedPtr<SVerticalBox>	 EditorPanels;

	/** The editors Animation Scrub Panel */
	TSharedPtr<class SAnimationScrubPanel> AnimScrubPanel;

	/** Tracks objects created for the details panel */
	FEditorObjectTracker EditorObjectTracker;

private:
	float GetPercentageInternal() const;
};
