// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "STileMapEditorViewportToolbar.h"

#define LOCTEXT_NAMESPACE "STileMapEditorViewportToolbar"

///////////////////////////////////////////////////////////
// STileMapEditorViewportToolbar

void STileMapEditorViewportToolbar::Construct(const FArguments& InArgs, TSharedPtr<class ICommonEditorViewportToolbarInfoProvider> InInfoProvider)
{
	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments(), InInfoProvider);
}

#undef LOCTEXT_NAMESPACE
