// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Components/InputKeySelector.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SInputKeySelector.h"

UInputKeySelector::UInputKeySelector( const FObjectInitializer& ObjectInitializer )
{
	SInputKeySelector::FArguments InputKeySelectorDefaults;
	SelectedKey = InputKeySelectorDefaults._SelectedKey.Get();
	ButtonStyle = InputKeySelectorDefaults._ButtonStyle;
	bAllowModifierKeys = InputKeySelectorDefaults._AllowModifierKeys;
}

void UInputKeySelector::SetSelectedKey( FInputChord InSelectedKey )
{
	if ( MyInputKeySelector.IsValid() )
	{
		MyInputKeySelector->SetSelectedKey( InSelectedKey );
	}
	SelectedKey = InSelectedKey;
}

void UInputKeySelector::SetKeySelectionText( FText InKeySelectionText )
{
	if ( MyInputKeySelector.IsValid() )
	{
		MyInputKeySelector->SetKeySelectionText( InKeySelectionText );
	}
	KeySelectionText = InKeySelectionText;
}

void UInputKeySelector::SetAllowModifierKeys( bool bInAllowModifierKeys )
{
	if ( MyInputKeySelector.IsValid() )
	{
		MyInputKeySelector->SetAllowModifierKeys( bInAllowModifierKeys );
	}
	bAllowModifierKeys = bInAllowModifierKeys;
}

bool UInputKeySelector::GetIsSelectingKey() const
{
	return MyInputKeySelector.IsValid() ? MyInputKeySelector->GetIsSelectingKey() : false;
}

void UInputKeySelector::SetButtonStyle( const FButtonStyle* InButtonStyle )
{
	if ( MyInputKeySelector.IsValid() )
	{
		MyInputKeySelector->SetButtonStyle( ButtonStyle );
	}
	ButtonStyle = InButtonStyle;
}

void UInputKeySelector::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyInputKeySelector->SetSelectedKey( SelectedKey );
	MyInputKeySelector->SetFont( Font );
	MyInputKeySelector->SetMargin( Margin );
	MyInputKeySelector->SetColorAndOpacity( ColorAndOpacity );
	MyInputKeySelector->SetButtonStyle( ButtonStyle );
	MyInputKeySelector->SetKeySelectionText( KeySelectionText );
	MyInputKeySelector->SetAllowModifierKeys( bAllowModifierKeys );
}

TSharedRef<SWidget> UInputKeySelector::RebuildWidget()
{
	MyInputKeySelector = SNew(SInputKeySelector)
		.SelectedKey(SelectedKey)
		.Font(Font)
		.Margin(Margin)
		.ColorAndOpacity(ColorAndOpacity)
		.ButtonStyle(ButtonStyle)
		.KeySelectionText(KeySelectionText)
		.AllowModifierKeys(bAllowModifierKeys)
		.OnKeySelected( BIND_UOBJECT_DELEGATE( SInputKeySelector::FOnKeySelected, HandleKeySelected ) )
		.OnIsSelectingKeyChanged( BIND_UOBJECT_DELEGATE( SInputKeySelector::FOnIsSelectingKeyChanged, HandleIsSelectingKeyChanged ) );
	return MyInputKeySelector.ToSharedRef();
}

void UInputKeySelector::HandleKeySelected(const FInputChord& InSelectedKey)
{
	SelectedKey = InSelectedKey;
	OnKeySelected.Broadcast(SelectedKey);
}

void UInputKeySelector::HandleIsSelectingKeyChanged()
{
	OnIsSelectingKeyChanged.Broadcast();
}
