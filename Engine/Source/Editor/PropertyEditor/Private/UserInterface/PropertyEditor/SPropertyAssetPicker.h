// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class SPropertyAssetPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SPropertyAssetPicker ) {}
		SLATE_EVENT( FOnGetAllowedClasses, OnGetAllowedClasses )
		SLATE_EVENT( FOnAssetSelected, OnAssetSelected )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );
private:

	FReply OnClicked();

	TSharedRef<SWidget> OnGenerateAssetPicker();

	void OnAssetSelectedFromPicker( const class FAssetData& AssetData );
private:
	/** Menu anchor for opening and closing the asset picker */
	TSharedPtr< class SMenuAnchor > AssetPickerAnchor;

	FOnGetAllowedClasses OnGetAllowedClasses;
	FOnAssetSelected OnAssetSelected;
};