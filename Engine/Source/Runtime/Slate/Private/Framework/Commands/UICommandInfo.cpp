// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"


FSimpleMulticastDelegate FBindingContext::CommandsChanged;


FUICommandInfoDecl FBindingContext::NewCommand( const FName InCommandName, const FText& InCommandLabel, const FText& InCommandDesc )
{
	return FUICommandInfoDecl( this->AsShared(), InCommandName, InCommandLabel, InCommandDesc );
}





FUICommandInfoDecl::FUICommandInfoDecl( const TSharedRef<FBindingContext>& InContext, const FName InCommandName, const FText& InLabel, const FText& InDesc )
	: Context( InContext )
{
	Info = MakeShareable( new FUICommandInfo( InContext->GetContextName() ) );
	Info->CommandName = InCommandName;
	Info->Label = InLabel;
	Info->Description = InDesc;
}

FUICommandInfoDecl& FUICommandInfoDecl::DefaultChord( const FInputChord& InDefaultChord )
{
	Info->DefaultChord = InDefaultChord;
	return *this;
}
FUICommandInfoDecl& FUICommandInfoDecl::UserInterfaceType( EUserInterfaceActionType::Type InType )
{
	Info->UserInterfaceType = InType;
	return *this;
}

FUICommandInfoDecl& FUICommandInfoDecl::Icon( const FSlateIcon& InIcon )
{
	Info->Icon = InIcon;
	return *this;
}

FUICommandInfoDecl& FUICommandInfoDecl::Description( const FText& InDescription )
{
	Info->Description = InDescription;
	return *this;
}

FUICommandInfoDecl::operator TSharedPtr<FUICommandInfo>() const
{
	FInputBindingManager::Get().CreateInputCommand( Context, Info.ToSharedRef() );
	return Info;
}

FUICommandInfoDecl::operator TSharedRef<FUICommandInfo>() const
{
	FInputBindingManager::Get().CreateInputCommand( Context, Info.ToSharedRef() );
	return Info.ToSharedRef();
}



const FText FUICommandInfo::GetInputText() const
{	
	return ActiveChord->GetInputText();
}


void FUICommandInfo::MakeCommandInfo( const TSharedRef<class FBindingContext>& InContext, TSharedPtr< FUICommandInfo >& OutCommand, const FName InCommandName, const FText& InCommandLabel, const FText& InCommandDesc, const FSlateIcon& InIcon, const EUserInterfaceActionType::Type InUserInterfaceType, const FInputChord& InDefaultChord )
{
	ensureMsgf( !InCommandLabel.IsEmpty(), TEXT("Command labels cannot be empty") );

	OutCommand = MakeShareable( new FUICommandInfo( InContext->GetContextName() ) );
	OutCommand->CommandName = InCommandName;
	OutCommand->Label = InCommandLabel;
	OutCommand->Description = InCommandDesc;
	OutCommand->Icon = InIcon;
	OutCommand->UserInterfaceType = InUserInterfaceType;
	OutCommand->DefaultChord = InDefaultChord;
	FInputBindingManager::Get().CreateInputCommand( InContext, OutCommand.ToSharedRef() );
}

void FUICommandInfo::SetActiveChord( const FInputChord& NewChord )
{
	ActiveChord->Set( NewChord );

	// Set the user defined chord for this command so it can be saved to disk later
	FInputBindingManager::Get().NotifyActiveChordChanged( *this );
}

void FUICommandInfo::RemoveActiveChord()
{
	// Chord already exists
	// Reset the other chord that has the same binding
	ActiveChord = MakeShareable( new FInputChord() );
	FInputBindingManager::Get().NotifyActiveChordChanged( *this );
}

TSharedRef<SToolTip> FUICommandInfo::MakeTooltip( const TAttribute<FText>& InText, const TAttribute< EVisibility >& InToolTipVisibility ) const
{
	return 
		SNew(SToolTip)
		.Visibility(InToolTipVisibility.IsBound() ? InToolTipVisibility : EVisibility::Visible)
		.Content()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(InText.IsBound() ? InText : GetDescription())
				.Font(FCoreStyle::Get().GetFontStyle( "ToolTip.Font" ))
				.ColorAndOpacity( FSlateColor::UseForeground() )
			]
			+SVerticalBox::Slot()
			[
				SNew(STextBlock)
				.Text(GetInputText())
				.Font(FCoreStyle::Get().GetFontStyle( "ToolTip.Font" ))
				.ColorAndOpacity( FSlateColor::UseSubduedForeground() )
			]
		];
}