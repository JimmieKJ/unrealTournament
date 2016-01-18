// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/UnrealEd/Public/SCommonEditorViewportToolbarBase.h"

///////////////////////////////////////////////////////////
// SMaterialEditorViewportToolBar

// In-viewport toolbar widget used in the material editor
class SMaterialEditorViewportToolBar : public SCommonEditorViewportToolbarBase
{
public:
	SLATE_BEGIN_ARGS(SMaterialEditorViewportToolBar) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class SMaterialEditor3DPreviewViewport> InViewport);

	// SCommonEditorViewportToolbarBase interface
	virtual TSharedRef<SWidget> GenerateShowMenu() const override;
	// End of SCommonEditorViewportToolbarBase
};

///////////////////////////////////////////////////////////
// SMaterialEditorViewportPreviewShapeToolBar

class SMaterialEditorViewportPreviewShapeToolBar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS(SMaterialEditorViewportPreviewShapeToolBar){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class SMaterialEditor3DPreviewViewport> InViewport);
};
