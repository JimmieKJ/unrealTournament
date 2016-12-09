// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "IPersonaPreviewScene.h"

class IEditableSkeleton;

//////////////////////////////////////////////////////////////////////////
// SRetargetManager

class SRetargetManager : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SRetargetManager )
	{}

	SLATE_END_ARGS()

	/**
	* Slate construction function
	*
	* @param InArgs - Arguments passed from Slate
	*
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, FSimpleMulticastDelegate& InOnPostUndo);

private:
	FReply OnViewRetargetBasePose();
	FReply OnResetRetargetBasePose();
	FReply OnSaveRetargetBasePose();

	/** The editable skeleton */
	TWeakPtr<class IEditableSkeleton> EditableSkeletonPtr;

	/** The preview scene  */
	TWeakPtr<class IPersonaPreviewScene> PreviewScenePtr;

	/** Delegate for undo/redo transaction **/
	void PostUndo();
	FText GetToggleRetargetBasePose() const;	
};
