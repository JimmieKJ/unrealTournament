// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Toolkits/SAssetEditorCommon.h"
#include "Toolkits/AssetEditorToolkit.h"


void SAssetEditorCommon::Construct( const FArguments& InArgs, TWeakPtr< FAssetEditorToolkit > InitParentToolkit, bool bIsPopup )
{
	// @todo toolkit major: This widget needs a proper design

	//	- Show dirty state? (already shown in SToolkitDisplay)

	//	- Show checked out/locked status/history
	//  - Open closed tabs?  Show open tab state (check?)
	//	- Revert (Close without Saving + GC/reload)
	//  - Reimport
	//	- Reload from disk
	//	- Show thumbnail
	//	- Show referencers, show what I reference
	//  - Show whether new version is available (option to Sync + Load)
	//	- Show who is currently editing file (if locked)
	//	- Undo/Redo access
	//	- Checkout/Save Now
	//  - Rename/Move?  Delete?  Probably not.

	const TSharedRef< FAssetEditorToolkit >& ParentToolkit = InitParentToolkit.Pin().ToSharedRef();

	const bool bShouldCloseWindowAfterMenuSelection = bIsPopup;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, ParentToolkit->GetToolkitCommands() );

	ParentToolkit->FillDefaultFileMenuCommands( MenuBuilder );
	ParentToolkit->FillDefaultAssetMenuCommands( MenuBuilder );

	this->ChildSlot
	[
		SNew( SVerticalBox )
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 1.0f )
			[
				MenuBuilder.MakeWidget()
			]
	];

}