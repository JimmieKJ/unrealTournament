// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

struct FCustomResetToDefault
{
	TAttribute<bool> IsResetToDefaultVisible;
	FSimpleDelegate OnResetToDefaultClicked;
};

class SResetToDefaultPropertyEditor : public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS( SResetToDefaultPropertyEditor )
		: _NonVisibleState( EVisibility::Hidden )
		{}
		SLATE_ARGUMENT( EVisibility, NonVisibleState )
		SLATE_ARGUMENT( FCustomResetToDefault, CustomResetToDefault )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedRef< class FPropertyEditor>& InPropertyEditor );


private:
	FText GetResetToolTip() const;

	EVisibility GetDiffersFromDefaultAsVisibility() const;

	void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	FReply OnDefaultResetClicked();
	FReply OnCustomResetClicked();

private:
	FCustomResetToDefault CustomResetToDefault;

	TSharedPtr< class FPropertyEditor > PropertyEditor;

	EVisibility NonVisibleState;

	bool bValueDiffersFromDefault;
};