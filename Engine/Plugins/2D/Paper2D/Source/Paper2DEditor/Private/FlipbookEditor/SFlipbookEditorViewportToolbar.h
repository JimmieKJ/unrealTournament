// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/UnrealEd/Public/SCommonEditorViewportToolbarBase.h"

// In-viewport toolbar widget used in the flipbook editor
class SFlipbookEditorViewportToolbar : public SCommonEditorViewportToolbarBase
{
public:
	SLATE_BEGIN_ARGS(SFlipbookEditorViewportToolbar) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class ICommonEditorViewportToolbarInfoProvider> InInfoProvider);

	// SCommonEditorViewportToolbarBase interface
	virtual TSharedRef<SWidget> GenerateShowMenu() const override;
	// End of SCommonEditorViewportToolbarBase
};
