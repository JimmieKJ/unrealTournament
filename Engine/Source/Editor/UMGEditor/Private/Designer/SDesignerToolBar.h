// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SViewportToolBar.h"

/**
 * 
 */
class SDesignerToolBar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS( SDesignerToolBar ){}
		SLATE_ARGUMENT( TSharedPtr<FUICommandList>, CommandList )
		SLATE_ARGUMENT( TSharedPtr<FExtender>, Extenders )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	/**
	 * Static: Creates a widget for the main tool bar
	 *
	 * @return	New widget
	 */
	TSharedRef< SWidget > MakeToolBar( const TSharedPtr< FExtender > InExtenders );	

private:
	// Begin Grid Snapping
	ECheckBoxState IsLocationGridSnapChecked() const;
	void HandleToggleLocationGridSnap(ECheckBoxState InState);
	FText GetLocationGridLabel() const;
	TSharedRef<SWidget> FillLocationGridSnapMenu();
	TSharedRef<SWidget> BuildLocationGridCheckBoxList(FName InExtentionHook, const FText& InHeading, const TArray<int32>& InGridSizes) const;
	static void SetGridSize(int32 InGridSize);
	static bool IsGridSizeChecked(int32 InGridSnapSize);
	// End Grid Snapping

	/** Command list */
	TSharedPtr<FUICommandList> CommandList;
};