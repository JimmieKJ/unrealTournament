// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "InputBindingEditorPrivatePCH.h"


#define LOCTEXT_NAMESPACE "SGestureEditor"


/* SGestureEditor interface
 *****************************************************************************/

void SGestureEditor::Construct( const FArguments& InArgs, TSharedPtr<FGestureTreeItem> InputCommand )
{
	bIsEditing = false;

	check( InputCommand->IsCommand() );

	CommandInfo = InputCommand->CommandInfo;
	OnEditBoxLostFocus = InArgs._OnEditBoxLostFocus;
	OnGestureChanged = InArgs._OnGestureChanged;
	OnEditingStopped = InArgs._OnEditingStopped;
	OnEditingStarted = InArgs._OnEditingStarted;

	static const FSlateFontInfo Font( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 9 );

	SEditableText::Construct( 
		SEditableText::FArguments() 
		.Text( this, &SGestureEditor::OnGetGestureInputText )
		.HintText(  this, &SGestureEditor::OnGetGestureInputHintText )
		.Font( Font )
	);

	LoadText();

}


TWeakPtr<SGestureEditor> SGestureEditor::GestureBeingEdited;


FReply SGestureEditor::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	const FKey Key = InKeyEvent.GetKey();

	if( bIsEditing ) 
	{

		TGuardValue<bool> TypingGuard( bIsTyping, true );

		if( !EKeys::IsModifierKey(Key) )
		{
			EditingInputGesture.Key = Key;
		}
	
		StartChangingText();

		EditingInputGesture.ModifierKeys = EModifierKey::FromBools(InKeyEvent.IsControlDown(), InKeyEvent.IsAltDown(), InKeyEvent.IsShiftDown(), InKeyEvent.IsCommandDown());

		LoadText();
		//@todo checking the length of localized string is not valid, at the very least in this manner [10/11/2013 justin.sargent]
		SetCaretPosition( EditingInputGesture.GetInputText().ToString().Len() );

		FinishChangingText();
		
		OnGestureTyped( EditingInputGesture );
		
		return FReply::Handled();
	}

	return FReply::Unhandled();
}


FReply SGestureEditor::OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	return FReply::Unhandled();
}


FReply SGestureEditor::OnKeyChar( const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent )
{
	return FReply::Unhandled();
}


FReply SGestureEditor::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& InMouseEvent )
{
	if ( InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !bIsEditing )
	{
		StartEditing();
		return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse);
	}

	return FReply::Unhandled();
}


FReply SGestureEditor::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	return FReply::Handled();
}


void SGestureEditor::OnFocusLost( const FFocusEvent& InFocusEvent )
{
	// Notify a listener that we lost focus so they can determine if we should still be in edit mode
	OnEditBoxLostFocus.ExecuteIfBound();
}


FText SGestureEditor::OnGetGestureInputText() const
{
	if( bIsEditing )
	{
		return EditingInputGesture.GetInputText();
	}
	else if( CommandInfo->GetActiveGesture()->IsValidGesture() )
	{
		return CommandInfo->GetActiveGesture()->GetInputText();
	}
	else
	{
		return FText::GetEmpty();
	}
}


FText SGestureEditor::OnGetGestureInputHintText() const
{
	const TSharedPtr<const FInputGesture>& ActiveGesture = CommandInfo->GetActiveGesture();

	if( !bIsEditing || !CommandInfo->GetDefaultGesture().IsValidGesture() )
	{
		return LOCTEXT("NewBindingHelpText_NoCurrentBinding", "Type a new binding");

	}
	else if( bIsEditing )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("InputCommandBinding"), CommandInfo->GetDefaultGesture().GetInputText() );
		return FText::Format( LOCTEXT("NewBindingHelpText_CurrentBinding", "Default: {InputCommandBinding}"), Args );
	}
	
	return FText::GetEmpty();
}


void SGestureEditor::CommitNewGesture()
{
	if( EditingInputGesture.IsValidGesture() )
	{
		OnGestureCommitted( EditingInputGesture );
	}
}


void SGestureEditor::RemoveActiveGesture()
{
	CommandInfo->RemoveActiveGesture();
}


void SGestureEditor::StartEditing()
{
	if( GestureBeingEdited.IsValid()  )
	{
		GestureBeingEdited.Pin()->StopEditing();
	}

	GestureBeingEdited = SharedThis( this );

	NotificationMessage = FText::GetEmpty();
	EditingInputGesture = FInputGesture( EKeys::Invalid, EModifierKey::None );
	bIsEditing = true;

	OnEditingStarted.ExecuteIfBound();
}


void SGestureEditor::StopEditing()
{
	if( GestureBeingEdited.IsValid() && GestureBeingEdited.Pin().Get() == this )
	{
		GestureBeingEdited.Reset();
	}

	OnEditingStopped.ExecuteIfBound();

	bIsEditing = false;

	EditingInputGesture = FInputGesture( EKeys::Invalid, EModifierKey::None );
	NotificationMessage = FText::GetEmpty();
}


void SGestureEditor::OnGestureTyped( const FInputGesture& NewGesture )
{
	// Check to see if a valid sequence was entered
	if( NewGesture.IsValidGesture() )
	{
		// The gesture doesn't exist in the command so now we need to make sure the gesture doesn't exist in another command in the same context
		FName ContextName = CommandInfo->GetBindingContext();

		const bool bCheckDefaultGesture = false;
		const TSharedPtr<FUICommandInfo> FoundDesc = FInputBindingManager::Get().GetCommandInfoFromInputGesture( ContextName, NewGesture, bCheckDefaultGesture );

		if( FoundDesc.IsValid() && FoundDesc->GetCommandName() != CommandInfo->GetCommandName() )
		{
			// Gesture already exists
			NotificationMessage = FText::Format( LOCTEXT("KeyAlreadyBound", "{0} is already bound to {1}"), NewGesture.GetInputText(), FoundDesc->GetLabel() );
		}
		else
		{

			NotificationMessage = FText::GetEmpty();
		}
	}

	OnGestureChanged.ExecuteIfBound();
}


void SGestureEditor::OnGestureCommitted( const FInputGesture& NewGesture )
{
	// This delegate is only called on valid gestures
	check( NewGesture.IsValidGesture() );

	{
		const FName ContextName = CommandInfo->GetBindingContext();

		const bool bCheckDefaultGesture = false;
		const TSharedPtr<FUICommandInfo> FoundDesc = FInputBindingManager::Get().GetCommandInfoFromInputGesture( ContextName, NewGesture, bCheckDefaultGesture );

		if( FoundDesc.IsValid() && FoundDesc->GetCommandName() != CommandInfo->GetCommandName() )
		{
			// Remove the active gesture on the command that was already bound to the gesture being set on another command.
			FoundDesc->RemoveActiveGesture();
		}

		// Set the new gesture on the command being edited
		CommandInfo->SetActiveGesture( NewGesture );
	}
}


#undef LOCTEXT_NAMESPACE
