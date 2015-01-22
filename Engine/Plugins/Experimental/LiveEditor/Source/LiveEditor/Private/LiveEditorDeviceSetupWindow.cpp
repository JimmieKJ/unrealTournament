// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LiveEditorPrivatePCH.h"
#include "LiveEditorDevice.h"
#include "LiveEditorDeviceSetupWindow.h"
#include "LiveEditorWizardBase.h"

DEFINE_LOG_CATEGORY_STATIC(LiveEditorDeviceSetupWindow, Log, All);

#define LOCTEXT_NAMESPACE "LiveEditorWizard"

void SLiveEditorWizardWindow::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SAssignNew( Root, SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew( STextBlock )
			.Text( this, &SLiveEditorWizardWindow::GetTitle )
			.ColorAndOpacity( FLinearColor::White )
			.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 18 ) )
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SAssignNew( DynamicContentPane, SBorder )
			.BorderImage( FEditorStyle::GetBrush( "ToolPanel.GroupBorder" ) )
			.Padding( 2.0f )
			[
				SNew( STextBlock )
				.Text( LOCTEXT("NoContent", "No Content") )
				.ColorAndOpacity( FLinearColor::White )
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		.VAlign( VAlign_Bottom )
		.HAlign( HAlign_Right )
		[
			SNew(SUniformGridPanel)
			.SlotPadding( FMargin( 5.0f ) )
			+SUniformGridPanel::Slot(0,0)
			.HAlign(HAlign_Right)
			[
				SNew( SButton )
				.Text( LOCTEXT("Cancel", "Cancel") )
				.Visibility( this, &SLiveEditorWizardWindow::ShowCancelButton )
				.OnClicked( this, &SLiveEditorWizardWindow::OnCancelClicked )
			]
			+SUniformGridPanel::Slot(1,0)
			.HAlign(HAlign_Right)
			[
				SNew( SButton )
				.Text( this, &SLiveEditorWizardWindow::GetButtonText )
				.IsEnabled( this, &SLiveEditorWizardWindow::ButtonEnabled )
				.OnClicked( this, &SLiveEditorWizardWindow::OnButtonClicked )
			]
		]
	];
}

void SLiveEditorWizardWindow::Activated()
{
	GenerateStateContent();
}

FText SLiveEditorWizardWindow::GetTitle() const
{
	FLiveEditorWizardBase *Wizard = FLiveEditorManager::Get().GetActiveWizard();
	check( Wizard != NULL );
	return Wizard->GetActiveStateTitle();
}

void SLiveEditorWizardWindow::GenerateStateContent()
{
	FLiveEditorWizardBase *Wizard = FLiveEditorManager::Get().GetActiveWizard();
	if ( Wizard )
	{
		DynamicContentPane.Get()->SetContent( Wizard->GenerateStateContent() );
	}
	else
	{
		DynamicContentPane.Get()->SetContent(
			SNew( STextBlock )
			.Text( LOCTEXT("NoContent", "No Content") )
			.ColorAndOpacity( FLinearColor::White ) );
	}
}

FText SLiveEditorWizardWindow::GetButtonText() const
{
	FLiveEditorWizardBase *Wizard = FLiveEditorManager::Get().GetActiveWizard();
	check( Wizard != NULL );
	return Wizard->GetAdvanceButtonText();
}

bool SLiveEditorWizardWindow::ButtonEnabled() const
{
	FLiveEditorWizardBase *Wizard = FLiveEditorManager::Get().GetActiveWizard();
	return (Wizard != NULL && Wizard->IsReadyToAdvance());
}

EVisibility SLiveEditorWizardWindow::ShowCancelButton() const
{
	FLiveEditorWizardBase *Wizard = FLiveEditorManager::Get().GetActiveWizard();
	return ( Wizard == NULL || (Wizard->IsOnLastStep() && ButtonEnabled()) )? EVisibility::Hidden : EVisibility::Visible;
}

FReply SLiveEditorWizardWindow::OnCancelClicked()
{
	FLiveEditorWizardBase *Wizard = FLiveEditorManager::Get().GetActiveWizard();
	check( Wizard != NULL );
	Wizard->Cancel();
	FLiveEditorManager::Get().SetActiveWizard(NULL);
	return FReply::Handled();
}

FReply SLiveEditorWizardWindow::OnButtonClicked()
{
	FLiveEditorWizardBase *Wizard = FLiveEditorManager::Get().GetActiveWizard();
	check( Wizard != NULL && Wizard->IsReadyToAdvance());
	if ( Wizard->AdvanceState() )
	{
		FLiveEditorManager::Get().SetActiveWizard(NULL);
	}
	GenerateStateContent();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
