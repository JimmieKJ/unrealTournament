// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "TaskBrowserPrivatePCH.h"
#include "STaskSettings.h"
#include "STaskBrowser.h"

#define LOCTEXT_NAMESPACE "STaskSettings"

//////////////////////////////////////////////////////////////////////////
// STaskSettings

/**
 * Construct the widget
 *
 * @param InArgs		A declaration from which to construct the widget
 */
void STaskSettings::Construct(const FArguments& InArgs)
{
	WidgetWindow = InArgs._WidgetWindow;

	// Load preferences
	FTaskBrowserSettings TBSettings;
	TBSettings.LoadSettings();

	// Standard paddings
	const FMargin BorderPadding( 6, 6 );			// Surrounding padding on the fields
	const FMargin TopBorderPadding( 6, 12, 6, 6 );	// As above, will additional padding at the top
	const FMargin ButtonPadding( 8, 4 );			// Padding around the button text
	const FMargin SmallPadding( 0, 0, 12, 0 );		// Padding to the right of the field's text
	const FMargin HeaderPadding( 6, 24, 0, 0 );		// Padding for the titles of the borders
	const float SettingWidth = 0.225f;
	const float FieldWidth = 135.0f;

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
				.Padding( HeaderPadding )
				[
					SNew(STextBlock) .Text( LOCTEXT("Connection", "Connection") )
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding( TopBorderPadding )
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.HAlign(HAlign_Right)
							.VAlign(VAlign_Center)
							.Padding( SmallPadding )
							.FillWidth( SettingWidth )
							[
								SNew(STextBlock) .Text( LOCTEXT("Server", "Server") )
							]
							+SHorizontalBox::Slot()
							.FillWidth( 0.5f )
							[
								SAssignNew(Server, SEditableTextBox) .Text( FText::FromString( TBSettings.ServerName ))
							]
							+SHorizontalBox::Slot()
							.HAlign(HAlign_Right)
							.VAlign(VAlign_Center)
							.Padding( SmallPadding )
							.FillWidth( 0.13f )
							[
								SNew(STextBlock) .Text( LOCTEXT("Port", "Port") )
							]
							+SHorizontalBox::Slot()
							.FillWidth( 0.10f )
							[
								SAssignNew(Port, SEditableTextBox) .Text( FText::AsNumber(TBSettings.ServerPort) )
							]
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding( BorderPadding )
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.HAlign(HAlign_Right)
							.VAlign(VAlign_Center)
							.Padding( SmallPadding )
							.FillWidth( SettingWidth )
							[
								SNew(STextBlock) .Text( LOCTEXT("Login", "Login") )
							]
							+SHorizontalBox::Slot()
							.MaxWidth( FieldWidth )
							.FillWidth( 1.0f - SettingWidth )
							[
								SAssignNew(Login, SEditableTextBox) .Text( FText::FromString( TBSettings.UserName ) )	.IsEnabled( !TBSettings.bUseSingleSignOn )
							]
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding( BorderPadding )
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.HAlign(HAlign_Right)
							.VAlign(VAlign_Center)
							.Padding( SmallPadding )
							.FillWidth( SettingWidth )
							[
								SNew(STextBlock) .Text( LOCTEXT("Password", "Password") )
							]
							+SHorizontalBox::Slot()
							.MaxWidth( FieldWidth )
							.FillWidth( 1.0f - SettingWidth )
							[
								SAssignNew(Password, SEditableTextBox) .Text( FText::FromString( TBSettings.Password ) ) .IsEnabled( !TBSettings.bUseSingleSignOn ) .IsPassword( true )
							]
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding( BorderPadding )
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.HAlign(HAlign_Right)
							.VAlign(VAlign_Center)
							.Padding( SmallPadding )
							.FillWidth( SettingWidth )
							[
								SNew(STextBlock) .Text( LOCTEXT("ProjectName", "Project name") )
							]
							+SHorizontalBox::Slot()
							.MaxWidth( FieldWidth )
							.FillWidth( 1.0f - SettingWidth )
							[
								SAssignNew(Project, SEditableTextBox) .Text( FText::FromString( TBSettings.ProjectName ))
							]
						]
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding( HeaderPadding )
				[
					SNew(STextBlock) .Text( LOCTEXT("Preferences", "Preferences") )
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding( TopBorderPadding )
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.AutoWidth()
							[
								SAssignNew(Autoconnect, SCheckBox)
								.IsChecked( TBSettings.bAutoConnectAtStartup ? ECheckBoxState::Checked : ECheckBoxState::Unchecked )
							]
							+SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Right)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock) .Text( LOCTEXT("AutoConnectAtStartup", "Auto connect at startup") )
							]
							+SHorizontalBox::Slot()	// Set this box to be as wide as possible, forcing the rest in this row to be right aligned
							.FillWidth( 1.0f )
							+SHorizontalBox::Slot()
							.AutoWidth()
							[
								SAssignNew(SingleSignOn, SCheckBox)
								.IsChecked( TBSettings.bUseSingleSignOn ? ECheckBoxState::Checked : ECheckBoxState::Unchecked )
								.OnCheckStateChanged( this, &STaskSettings::OnSingleSignOnChanged )
							]
							+SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Right)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock) .Text( LOCTEXT("UseSingleSignOn", "Use single sign-on") )
							]
						]
					]
				]
				
				+SVerticalBox::Slot()
				.FillHeight(1)

				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
					+SUniformGridPanel::Slot(0,0)
					[
						SAssignNew(OK, SButton)
						.ContentPadding( FEditorStyle::GetMargin("StandardDialog.ContentPadding") )
						.HAlign(HAlign_Center)
						.OnClicked( this, &STaskSettings::OnOKClicked )
						[
							SNew(STextBlock) .Text( LOCTEXT("OK", "OK") )
						]
					]
					+SUniformGridPanel::Slot(1,0)
					[
						SAssignNew(Cancel, SButton)
						.ContentPadding( FEditorStyle::GetMargin("StandardDialog.ContentPadding") )
						.HAlign(HAlign_Center)
						.OnClicked( this, &STaskSettings::OnCancelClicked )
						[
							SNew(STextBlock) .Text( LOCTEXT("Cancel", "Cancel") )
						]
					]
				]
			]
		]
	];
}

void STaskSettings::OnSingleSignOnChanged( const ECheckBoxState NewCheckedState )
{
	// Enable/Disable the login and password fields
	const bool bEnabled = ( NewCheckedState == ECheckBoxState::Checked ? false : true );
	Login->SetEnabled( bEnabled );
	Password->SetEnabled( bEnabled );
}

FReply STaskSettings::OnOKClicked()
{
	// Load preferences
	FTaskBrowserSettings TBSettings;
	TBSettings.LoadSettings();

	// Store new preferences from controls
	TBSettings.ServerName = Server->GetText().ToString();
	TBSettings.ServerPort = FCString::Atoi( *Port->GetText().ToString() );
	TBSettings.UserName = Login->GetText().ToString();
	TBSettings.Password = Password->GetText().ToString();
	TBSettings.ProjectName = Project->GetText().ToString();
	TBSettings.bAutoConnectAtStartup = Autoconnect->IsChecked();
	TBSettings.bUseSingleSignOn = SingleSignOn->IsChecked();

	// Save preferences to disk
	TBSettings.SaveSettings();

	// Close the window
	if ( WidgetWindow.IsValid() )
	{
		WidgetWindow.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

FReply STaskSettings::OnCancelClicked()
{
	// Close the window
	if ( WidgetWindow.IsValid() )
	{
		WidgetWindow.Pin()->RequestDestroyWindow();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
