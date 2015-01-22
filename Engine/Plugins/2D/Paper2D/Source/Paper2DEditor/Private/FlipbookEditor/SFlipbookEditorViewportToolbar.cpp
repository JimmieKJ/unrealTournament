// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "SFlipbookEditorViewportToolbar.h"

#define LOCTEXT_NAMESPACE "SFlipbookEditorViewportToolbar"

///////////////////////////////////////////////////////////
// SFlipbookEditorViewportToolbar

void SFlipbookEditorViewportToolbar::Construct(const FArguments& InArgs, TSharedPtr<class ICommonEditorViewportToolbarInfoProvider> InInfoProvider)
{
	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments(), InInfoProvider);
}

#undef LOCTEXT_NAMESPACE
