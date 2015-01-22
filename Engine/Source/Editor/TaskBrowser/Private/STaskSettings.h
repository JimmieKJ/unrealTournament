// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// STaskSettings

class STaskSettings : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( STaskSettings )
	{
	}
		SLATE_ARGUMENT( TSharedPtr<SWindow>, WidgetWindow )
	SLATE_END_ARGS()

	/**
	 * Construct the widget
	 *
	 * @param	InArgs			A declaration from which to construct the widget
	 */
	void Construct(const FArguments& InArgs);

private:

	/** Handle when the Single Sign On flag is checked */
	void OnSingleSignOnChanged( const ECheckBoxState NewCheckedState );

	/** Handle when the OK button is pressed */
	FReply OnOKClicked();

	/** Handle when the Cancel button is pressed */
	FReply OnCancelClicked();

	/** The parent window */
	TWeakPtr<SWindow> WidgetWindow;

	/** The Server text box */
	TSharedPtr<SEditableTextBox> Server;

	/** The Port text box */
	TSharedPtr<SEditableTextBox> Port;

	/** The Login text box */
	TSharedPtr<SEditableTextBox> Login;

	/** The Password text box */
	TSharedPtr<SEditableTextBox> Password;

	/** The Project text box */
	TSharedPtr<SEditableTextBox> Project;

	/** The Autoconnect tick box */
	TSharedPtr<SCheckBox> Autoconnect;

	/** The SingleSignOn tick box */
	TSharedPtr<SCheckBox> SingleSignOn;

	/** The OK button */
	TSharedPtr<SButton> OK;	

	/** The Cancel button */
	TSharedPtr<SButton> Cancel;	
};