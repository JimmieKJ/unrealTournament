// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "SEditorViewportToolBarMenu.h"

class SEditorViewport;

class UNREALED_API SEditorViewportViewMenu : public SEditorViewportToolbarMenu
{
public:
	SLATE_BEGIN_ARGS( SEditorViewportViewMenu ){}
		SLATE_ARGUMENT( TSharedPtr<class FExtender>, MenuExtenders )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedRef<SEditorViewport> InViewport, TSharedRef<class SViewportToolBar> InParentToolBar );

private:
	FText GetViewMenuLabel() const;
	const FSlateBrush* GetViewMenuLabelIcon() const;

protected:
	virtual TSharedRef<SWidget> GenerateViewMenuContent() const;
	TWeakPtr<SEditorViewport> Viewport;

private:
	TSharedPtr<class FExtender> MenuExtenders;
};