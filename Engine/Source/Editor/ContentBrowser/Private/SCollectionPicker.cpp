// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserPCH.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

void SCollectionPicker::Construct( const FArguments& InArgs )
{
	ChildSlot
	[
		SNew(SCollectionView)
		.AllowCollectionButtons(InArgs._CollectionPickerConfig.AllowCollectionButtons)
		.OnCollectionSelected(InArgs._CollectionPickerConfig.OnCollectionSelected)
		.AllowCollapsing( false )
		.AllowContextMenu( InArgs._CollectionPickerConfig.AllowRightClickMenu )
	];
}

#undef LOCTEXT_NAMESPACE