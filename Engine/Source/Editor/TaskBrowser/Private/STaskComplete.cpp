// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "TaskBrowserPrivatePCH.h"
#include "STaskComplete.h"
#include "STaskBrowser.h"
#include "STextComboBox.h"

#define LOCTEXT_NAMESPACE "STaskComplete"

//////////////////////////////////////////////////////////////////////////
// STaskComplete

/**
 * Construct the widget
 *
 * @param InArgs		A declaration from which to construct the widget
 */
void STaskComplete::Construct(const FArguments& InArgs)
{
	WidgetWindow = InArgs._WidgetWindow;
	ResolutionData = InArgs._ResolutionData;

	// HACK Convert our const array to a slate usable local type, and set the default selection if it's available
	TSharedPtr<FString> ResolutionSelection = NULL; 
	const FString DefaultResolution( TEXT( "Code/Content Change" ) );	// Not localized, intentionally
	for ( int32 iResolution = 0; iResolution < InArgs._ResolutionValues->Num(); iResolution++ )
	{
		const FString& ResolutionValue = (*InArgs._ResolutionValues)[ iResolution ];
		TSharedPtr<FString> ResolutionOption = MakeShareable(new FString(ResolutionValue));
		if (ResolutionValue == DefaultResolution)
		{
			ResolutionSelection = ResolutionOption;
		}
		ResolutionOptions.Add( ResolutionOption );
	}

	// Standard paddings
	const FMargin ButtonPadding( 8, 4 );			// Padding around the button text
	const FMargin SmallPadding( 0, 0, 12, 0 );		// Padding to the right of the field's text
	const FMargin TopPadding( 0, 20, 0, 5 );
	const FMargin NormalPadding( 0, 0, 0, 5 );
	const FMargin BottomPadding( 0, 20, 0, 0 );

	this->ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding( TopPadding )
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding( SmallPadding )
					.FillWidth( 0.18f )
					[
						SNew(STextBlock) .Text( LOCTEXT("Resolution", "Resolution") )
					]
					+SHorizontalBox::Slot()
					.FillWidth( 0.33f )
					[
						SAssignNew(Resolution, STextComboBox)
						.OptionsSource( &ResolutionOptions )
					]
					+SHorizontalBox::Slot()	// Dummy box
					.FillWidth( 0.49f )
				]
				+SVerticalBox::Slot()
				.Padding( NormalPadding )
				.FillHeight(1.0f)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Top)
					.Padding( SmallPadding )
					.FillWidth( 0.18f )
					[
						SNew(STextBlock) .Text( LOCTEXT("Comments", "Comments") )
					]
					+SHorizontalBox::Slot()
					.FillWidth( 0.82f )
					[
						SAssignNew(Comments, SEditableTextBox)
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding( NormalPadding )
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding( SmallPadding )
					.FillWidth( 0.184f )
					[
						SNew(STextBlock) .Text( LOCTEXT("Changelist", "Changelist") )
					]
					+SHorizontalBox::Slot()
					.FillWidth( 0.17f )
					[
						SAssignNew(Changelist, SEditableTextBox)
					]
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding( SmallPadding )
					.FillWidth( 0.408f )
					[
						SNew(STextBlock) .Text( LOCTEXT("TimeToComplete", "Time to complete (hours)") )
					]
					+SHorizontalBox::Slot()
					.FillWidth( 0.08f )
					[
						SAssignNew(Time, SEditableTextBox)
					]
					+SHorizontalBox::Slot()	// Dummy box
					.FillWidth( 0.158f )
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
					+SUniformGridPanel::Slot(0,0)
					[
						SAssignNew(OK, SButton)
						.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
						.HAlign(HAlign_Center)
						.OnClicked( this, &STaskComplete::OnOKClicked )
						[
							SNew(STextBlock) .Text( LOCTEXT("OK", "OK") )
						]
					]
					+SUniformGridPanel::Slot(1,0)
					[
						SAssignNew(Cancel, SButton)
						.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
						.HAlign(HAlign_Center)
						.OnClicked( this, &STaskComplete::OnCancelClicked )
						[
							SNew(STextBlock) .Text( LOCTEXT("Cancel", "Cancel") )
						]
					]
				]
			]
		]
	];

	// If we found our preferred selection choice, pick it
	if ( ResolutionSelection.IsValid() )
	{
		Resolution->SetSelectedItem( ResolutionSelection );
	}
}

FReply STaskComplete::OnOKClicked()
{
	// Make sure the user entered something for each
	TSharedPtr<FString> ResolutionSelection = Resolution->GetSelectedItem();
	if( ( !ResolutionSelection.IsValid() || ResolutionSelection->Len() == 0 ) ||
		( Comments->GetText().IsEmpty() ) ||
		( Changelist->GetText().IsNumeric() == false ) ||
		( Time->GetText().IsNumeric() == false ) )
	{
		// At least one field is empty, so notify the user and veto the OK button event
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "TaskBrowser_Error_NeedValidDataToMarkComplete", "Sorry, the task cannot be marked as complete until you've entered valid information for all of the fields.  Please complete the form before pressing the OK button." ) );
	}
	else
	{
		// Store data from controls
		ResolutionData->ResolutionType = *ResolutionSelection.Get();
		ResolutionData->Comments = Comments->GetText().ToString();
		ResolutionData->ChangelistNumber = FCString::Atoi( *Changelist->GetText().ToString() );
		ResolutionData->HoursToComplete = static_cast< double >( FCString::Atof( *Time->GetText().ToString() ) );

		// Close the window
		if ( WidgetWindow.IsValid() )
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
	}
	return FReply::Handled();
}

FReply STaskComplete::OnCancelClicked()
{
	// Close the window
	if ( WidgetWindow.IsValid() )
	{
		WidgetWindow.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
