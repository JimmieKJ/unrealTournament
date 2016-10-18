// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "Persona.h"
#include "GraphEditor.h"
#include "SNodePanel.h"
#include "SAnimCurvePanel.h"
#include "SAnimEditorBase.h"

//////////////////////////////////////////////////////////////////////////
// SSequenceEditor

/** Overall animation sequence editing widget */
class SSequenceEditor : public SAnimEditorBase
{
public:
	SLATE_BEGIN_ARGS( SSequenceEditor )
		: _Persona()
		, _Sequence(NULL)
		{}

		SLATE_ARGUMENT( TSharedPtr<FPersona>, Persona )
		SLATE_ARGUMENT( UAnimSequenceBase*, Sequence )
	SLATE_END_ARGS()

private:
	/** Persona reference **/
	TSharedPtr<class SAnimNotifyPanel>	AnimNotifyPanel;
	TSharedPtr<class SAnimCurvePanel>	AnimCurvePanel;
	TSharedPtr<class SAnimTrackCurvePanel>	AnimTrackCurvePanel;
	TSharedPtr<class SAnimationScrubPanel> AnimScrubPanel;

public:
	void Construct(const FArguments& InArgs);
	virtual ~SSequenceEditor();

	virtual UAnimationAsset* GetEditorObject() const override { return SequenceObj; }

private:
	/** Pointer to the animation sequence being edited */
	UAnimSequenceBase* SequenceObj;

	/** Post undo **/
	void PostUndo();
	void OnTrackCurveChanged();
};
