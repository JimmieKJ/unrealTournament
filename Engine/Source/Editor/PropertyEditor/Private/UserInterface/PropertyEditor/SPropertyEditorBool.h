// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class SPropertyEditorBool : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SPropertyEditorBool ) {}
	SLATE_END_ARGS()

	static bool Supports( const TSharedRef< class FPropertyEditor >& PropertyEditor );

	void Construct( const FArguments& InArgs, const TSharedRef< class FPropertyEditor >& PropertyEditor );

	void GetDesiredWidth( float& OutMinDesiredWidth, float& OutMaxDesiredWidth );

	// SWidget overrides
	virtual bool SupportsKeyboardFocus() const override;
	virtual bool HasKeyboardFocus() const override;
	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override;

private:

	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;

	ECheckBoxState OnGetCheckState() const;

	void OnCheckStateChanged( ECheckBoxState InNewState );

	/** @return True if the property can be edited */
	bool CanEdit() const;
private:

	TSharedPtr< class FPropertyEditor > PropertyEditor;

	TSharedPtr< class SCheckBox> CheckBox;
};
