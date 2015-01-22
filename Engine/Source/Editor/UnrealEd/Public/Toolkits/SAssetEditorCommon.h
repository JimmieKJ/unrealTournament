// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Asset Editor Common widget (exposes features that all asset editors need.)
 */
class UNREALED_API SAssetEditorCommon : public SCompoundWidget
{
	
public:

	SLATE_BEGIN_ARGS( SAssetEditorCommon ) {}
	SLATE_END_ARGS()

	/** Constructs this widget.  Called by Slate declarative syntax. */
	void Construct( const FArguments& InArgs, TWeakPtr< class FAssetEditorToolkit > InitParentToolkit, bool bIsPopup );


protected:

	// ...
};

