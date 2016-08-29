// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "InputBindingEditorPrivatePCH.h"
#include "SlateEditableTextLayout.h"

#define LOCTEXT_NAMESPACE "SChordEditor"


/* SChordEditor interface
 *****************************************************************************/

void SChordEditor::Construct( const FArguments& InArgs, TSharedPtr<FChordTreeItem> InputCommand )
{
	bIsEditing = false;

	check( InputCommand->IsCommand() );

	CommandInfo = InputCommand->CommandInfo;
	OnEditBoxLostFocus = InArgs._OnEditBoxLostFocus;
	OnChordChanged = InArgs._OnChordChanged;
	OnEditingStopped = InArgs._OnEditingStopped;
	OnEditingStarted = InArgs._OnEditingStarted;

	static const FSlateFontInfo RobotoFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 9 );

	SEditableText::Construct( 
		SEditableText::FArguments() 
		.Text( this, &SChordEditor::OnGetChordInputText )
		.HintText(  this, &SChordEditor::OnGetChordInputHintText )
		.Font( RobotoFont )
	);

	EditableTextLayout->LoadText();
}


TWeakPtr<SChordEditor> SChordEditor::ChordBeingEdited;


FReply SChordEditor::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	const FKey Key = InKeyEvent.GetKey();

	if( bIsEditing ) 
	{

		TGuardValue<bool> TypingGuard( bIsTyping, true );

		if( !EKeys::IsModifierKey(Key) )
		{
			EditingInputChord.Key = Key;
		}
	
		EditableTextLayout->BeginEditTransation();

		EditingInputChord.bCtrl = InKeyEvent.IsControlDown();
		EditingInputChord.bAlt = InKeyEvent.IsAltDown();
		EditingInputChord.bShift = InKeyEvent.IsShiftDown();
		EditingInputChord.bCmd = InKeyEvent.IsCommandDown();

		EditableTextLayout->LoadText();
		EditableTextLayout->GoTo(ETextLocation::EndOfDocument);

		EditableTextLayout->EndEditTransaction();
		
		OnChordTyped( EditingInputChord );
		
		return FReply::Handled();
	}

	return FReply::Unhandled();
}


FReply SChordEditor::OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	return FReply::Unhandled();
}


FReply SChordEditor::OnKeyChar( const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent )
{
	return FReply::Unhandled();
}


FReply SChordEditor::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& InMouseEvent )
{
	if ( InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !bIsEditing )
	{
		StartEditing();
		return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse);
	}

	return FReply::Unhandled();
}


FReply SChordEditor::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	return FReply::Handled();
}


void SChordEditor::OnFocusLost( const FFocusEvent& InFocusEvent )
{
	SEditableText::OnFocusLost(InFocusEvent);
	// Notify a listener that we lost focus so they can determine if we should still be in edit mode
	OnEditBoxLostFocus.ExecuteIfBound();
}


FText SChordEditor::OnGetChordInputText() const
{
	if( bIsEditing )
	{
		return EditingInputChord.GetInputText();
	}
	else if( CommandInfo->GetActiveChord()->IsValidChord() )
	{
		return CommandInfo->GetActiveChord()->GetInputText();
	}
	else
	{
		return FText::GetEmpty();
	}
}


FText SChordEditor::OnGetChordInputHintText() const
{
	const TSharedPtr<const FInputChord>& ActiveChord = CommandInfo->GetActiveChord();

	if( !bIsEditing || !CommandInfo->GetDefaultChord().IsValidChord() )
	{
		return LOCTEXT("NewBindingHelpText_NoCurrentBinding", "Type a new binding");

	}
	else if( bIsEditing )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("InputCommandBinding"), CommandInfo->GetDefaultChord().GetInputText() );
		return FText::Format( LOCTEXT("NewBindingHelpText_CurrentBinding", "Default: {InputCommandBinding}"), Args );
	}
	
	return FText::GetEmpty();
}


void SChordEditor::CommitNewChord()
{
	if( EditingInputChord.IsValidChord() )
	{
		OnChordCommitted( EditingInputChord );
	}
}


void SChordEditor::RemoveActiveChord()
{
	CommandInfo->RemoveActiveChord();
}


void SChordEditor::StartEditing()
{
	if( ChordBeingEdited.IsValid()  )
	{
		ChordBeingEdited.Pin()->StopEditing();
	}

	ChordBeingEdited = SharedThis( this );

	NotificationMessage = FText::GetEmpty();
	EditingInputChord = FInputChord( EKeys::Invalid, EModifierKey::None );
	bIsEditing = true;

	OnEditingStarted.ExecuteIfBound();
}


void SChordEditor::StopEditing()
{
	if( ChordBeingEdited.IsValid() && ChordBeingEdited.Pin().Get() == this )
	{
		ChordBeingEdited.Reset();
	}

	OnEditingStopped.ExecuteIfBound();

	bIsEditing = false;

	EditingInputChord = FInputChord( EKeys::Invalid, EModifierKey::None );
	NotificationMessage = FText::GetEmpty();
}


void SChordEditor::OnChordTyped( const FInputChord& NewChord )
{
	// Check to see if a valid sequence was entered
	if( NewChord.IsValidChord() )
	{
		// The chord doesn't exist in the command so now we need to make sure the chord doesn't exist in another command in the same context
		FName ContextName = CommandInfo->GetBindingContext();

		const bool bCheckDefaultChord = false;
		const TSharedPtr<FUICommandInfo> FoundDesc = FInputBindingManager::Get().GetCommandInfoFromInputChord( ContextName, NewChord, bCheckDefaultChord );

		if( FoundDesc.IsValid() && FoundDesc->GetCommandName() != CommandInfo->GetCommandName() )
		{
			// Chord already exists
			NotificationMessage = FText::Format( LOCTEXT("KeyAlreadyBound", "{0} is already bound to {1}"), NewChord.GetInputText(), FoundDesc->GetLabel() );
		}
		else
		{

			NotificationMessage = FText::GetEmpty();
		}
	}

	OnChordChanged.ExecuteIfBound();
}


void SChordEditor::OnChordCommitted( const FInputChord& NewChord )
{
	// This delegate is only called on valid chords
	check( NewChord.IsValidChord() );

	{
		const FName ContextName = CommandInfo->GetBindingContext();

		const bool bCheckDefaultChord = false;
		const TSharedPtr<FUICommandInfo> FoundDesc = FInputBindingManager::Get().GetCommandInfoFromInputChord( ContextName, NewChord, bCheckDefaultChord );

		if( FoundDesc.IsValid() && FoundDesc->GetCommandName() != CommandInfo->GetCommandName() )
		{
			// Remove the active chord on the command that was already bound to the chord being set on another command.
			FoundDesc->RemoveActiveChord();
		}

		// Set the new chord on the command being edited
		CommandInfo->SetActiveChord( NewChord );
	}
}


#undef LOCTEXT_NAMESPACE
