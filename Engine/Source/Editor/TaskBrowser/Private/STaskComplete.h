// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TaskDatabaseDefs.h"

//////////////////////////////////////////////////////////////////////////
// STaskComplete

class STaskComplete : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( STaskComplete )
	{
	}
		SLATE_ARGUMENT( TSharedPtr<SWindow>, WidgetWindow )
		SLATE_ARGUMENT( FTaskResolutionData*, ResolutionData )
		SLATE_ARGUMENT( const TArray< FString >*, ResolutionValues )
	SLATE_END_ARGS()

	/**
	 * Construct the widget
	 *
	 * @param	InArgs			A declaration from which to construct the widget
	 */
	void Construct(const FArguments& InArgs);

private:

	/** Handle when the OK button is pressed */
	FReply OnOKClicked();

	/** Handle when the Cancel button is pressed */
	FReply OnCancelClicked();

	/** The parent window */
	TWeakPtr<SWindow> WidgetWindow;

	/** The resolution to apply to the tasks */
	FTaskResolutionData* ResolutionData;

	/** The usable copy of the choices in ResolutionValues*/
	TArray< TSharedPtr<FString> > ResolutionOptions;

	/** The Resolution dropdown */
	TSharedPtr<STextComboBox> Resolution;

	/** The Comments text box */
	TSharedPtr<SEditableTextBox> Comments;

	/** The Changelist text box */
	TSharedPtr<SEditableTextBox> Changelist;

	/** The Time text box */
	TSharedPtr<SEditableTextBox> Time;

	/** The OK button */
	TSharedPtr<SButton> OK;	

	/** The Cancel button */
	TSharedPtr<SButton> Cancel;	
};