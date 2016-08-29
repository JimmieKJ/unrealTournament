// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

// Forward decl
class FResetToDefaultOverride;

/** Widget showing the reset to default value button */
class SResetToDefaultPropertyEditor : public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS( SResetToDefaultPropertyEditor )
		: _NonVisibleState( EVisibility::Hidden )
		{}
		SLATE_ARGUMENT( EVisibility, NonVisibleState )
		SLATE_ARGUMENT(TOptional<FResetToDefaultOverride>, CustomResetToDefault)
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedRef< class FPropertyEditor>& InPropertyEditor );


private:
	FText GetResetToolTip() const;

	EVisibility GetDiffersFromDefaultAsVisibility() const;

	void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	FReply OnDefaultResetClicked();
	FReply OnCustomResetClicked();

	void UpdateDiffersFromDefaultState();
	void OnPropertyValueChanged();
private:
	TOptional<FResetToDefaultOverride> OptionalCustomResetToDefault;

	TSharedPtr< class FPropertyEditor > PropertyEditor;

	EVisibility NonVisibleState;

	bool bValueDiffersFromDefault;
};