// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "SInlineEditableTextBlock.h"


void SInlineEditableTextBlock::Construct( const FArguments& InArgs )
{
	check(InArgs._Style);

	OnBeginTextEditDelegate = InArgs._OnBeginTextEdit;
	OnTextCommittedDelegate = InArgs._OnTextCommitted;
	IsSelected = InArgs._IsSelected;
	OnVerifyTextChanged= InArgs._OnVerifyTextChanged;
	Text = InArgs._Text;
	bIsReadOnly = InArgs._IsReadOnly;
	DoubleSelectDelay = 0.0f;

	ChildSlot
	[
		SAssignNew(HorizontalBox, SHorizontalBox)
			
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			SAssignNew(TextBlock, STextBlock)
			.Text(Text)
			.TextStyle( &InArgs._Style->TextStyle )
			.Font(InArgs._Font)
			.ColorAndOpacity( InArgs._ColorAndOpacity )
			.ShadowColorAndOpacity( InArgs._ShadowColorAndOpacity )
			.ShadowOffset( InArgs._ShadowOffset )
			.HighlightText( InArgs._HighlightText )
			.ToolTipText( InArgs._ToolTipText )
			.WrapTextAt( InArgs._WrapTextAt )
			.Justification( InArgs._Justification )
			.LineBreakPolicy( InArgs._LineBreakPolicy )
		]
	];

	SAssignNew(TextBox, SEditableTextBox)
		.Text(InArgs._Text)
		.Style(&InArgs._Style->EditableTextBoxStyle)
		.Font(InArgs._Font)
		.ToolTipText( InArgs._ToolTipText )
		.OnTextChanged( this, &SInlineEditableTextBlock::OnTextChanged )
		.OnTextCommitted(this, &SInlineEditableTextBlock::OnTextBoxCommitted)
		.SelectAllTextWhenFocused(true)
		.ClearKeyboardFocusOnCommit(false);
}

SInlineEditableTextBlock::~SInlineEditableTextBlock()
{
	if(IsInEditMode())
	{
		// Clear the error so it will vanish.
		TextBox->SetError( FText::GetEmpty() );
	}
}

void SInlineEditableTextBlock::CancelEditMode()
{
	ExitEditingMode();

	// Get the text from source again.
	TextBox->SetText(Text);
}

bool SInlineEditableTextBlock::SupportsKeyboardFocus() const
{
	//Can not have keyboard focus if it's status of being selected is managed by another widget.
	return !IsSelected.IsBound();
}

void SInlineEditableTextBlock::EnterEditingMode()
{
	if(!bIsReadOnly.Get() && FSlateApplication::Get().HasAnyMouseCaptor() == false)
	{
		if(TextBlock->GetVisibility() == EVisibility::Visible)
		{
			HorizontalBox->AddSlot()
				[
					TextBox.ToSharedRef()
				];

			WidgetToFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
			FSlateApplication::Get().SetKeyboardFocus(TextBox, EFocusCause::SetDirectly);

			TextBlock->SetVisibility(EVisibility::Collapsed);

			OnBeginTextEditDelegate.ExecuteIfBound(TextBox->GetText());
		}
	}
}

void SInlineEditableTextBlock::ExitEditingMode()
{
	HorizontalBox->RemoveSlot(TextBox.ToSharedRef());
	TextBlock->SetVisibility(EVisibility::Visible);

	// Clear the error so it will vanish.
	TextBox->SetError( FText::GetEmpty() );

	// Restore the original widget focus
	TSharedPtr<SWidget> WidgetToFocusPin = WidgetToFocus.Pin();
	if(WidgetToFocusPin.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(WidgetToFocusPin, EFocusCause::SetDirectly);
	}
	else
	{
		FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
	}
}

bool SInlineEditableTextBlock::IsInEditMode() const
{
	return TextBlock->GetVisibility() == EVisibility::Collapsed;
}

void SInlineEditableTextBlock::SetText( const TAttribute< FText >& InText )
{
	Text = InText;
	TextBlock->SetText( Text );
	TextBox->SetText( Text );
}

void SInlineEditableTextBlock::SetText( const FString& InText )
{
	Text = FText::FromString( InText );
	TextBlock->SetText( Text );
	TextBox->SetText( Text );
}

void SInlineEditableTextBlock::SetWrapTextAt( const TAttribute<float>& InWrapTextAt )
{
	TextBlock->SetWrapTextAt( InWrapTextAt );
}

FReply SInlineEditableTextBlock::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if( !MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) || MouseEvent.IsControlDown() || MouseEvent.IsShiftDown())
	{
		return FReply::Unhandled();
	}

	if(IsSelected.IsBound())
	{
		if(IsSelected.Execute() && !bIsReadOnly.Get() && !ActiveTimerHandle.IsValid())
		{
			RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SInlineEditableTextBlock::TriggerEditMode));
		}
	}
	else
	{
		// The widget is not managed by another widget, so handle the mouse input and enter edit mode if ready.
		if(HasKeyboardFocus())
		{
			EnterEditingMode();
		}
		return FReply::Handled();
	}

	// Do not handle the mouse input, this will allow for drag and dropping events to trigger.
	return FReply::Unhandled();
}

FReply SInlineEditableTextBlock::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	// Cancel during a drag over event, otherwise the widget may enter edit mode.
	auto PinnedActiveTimerHandle = ActiveTimerHandle.Pin();
	if (PinnedActiveTimerHandle.IsValid())
	{
		UnRegisterActiveTimer(PinnedActiveTimerHandle.ToSharedRef());
	}
	return FReply::Unhandled();
}

FReply SInlineEditableTextBlock::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	auto PinnedActiveTimerHandle = ActiveTimerHandle.Pin();
	if (PinnedActiveTimerHandle.IsValid())
	{
		UnRegisterActiveTimer(PinnedActiveTimerHandle.ToSharedRef());
	}
	return FReply::Unhandled();
}

EActiveTimerReturnType SInlineEditableTextBlock::TriggerEditMode(double InCurrentTime, float InDeltaTime)
{
	EnterEditingMode();
	return EActiveTimerReturnType::Stop;
}

FReply SInlineEditableTextBlock::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if(InKeyEvent.GetKey() == EKeys::F2)
	{
		EnterEditingMode();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SInlineEditableTextBlock::OnTextChanged(const FText& InText)
{
	if(IsInEditMode())
	{
		FText OutErrorMessage;

		if(OnVerifyTextChanged.IsBound() && !OnVerifyTextChanged.Execute(InText, OutErrorMessage))
		{
			TextBox->SetError( OutErrorMessage );
		}
		else
		{
			TextBox->SetError( FText::GetEmpty() );
		}
	}
}

void SInlineEditableTextBlock::OnTextBoxCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	if(InCommitType == ETextCommit::OnCleared)
	{
		CancelEditMode();
		// Commit the name, certain actions might need to be taken by the bound function
		OnTextCommittedDelegate.ExecuteIfBound(Text.Get(), InCommitType);
	}
	else if(IsInEditMode())
	{
		if(OnVerifyTextChanged.IsBound())
		{
			if(InCommitType == ETextCommit::OnEnter)
			{
				FText OutErrorMessage;
				if(!OnVerifyTextChanged.Execute(InText, OutErrorMessage))
				{
					// Display as an error.
					TextBox->SetError( OutErrorMessage );
					return;
				}
			}
			else if(InCommitType == ETextCommit::OnUserMovedFocus)
			{
				FText OutErrorMessage;
				if(!OnVerifyTextChanged.Execute(InText, OutErrorMessage))
				{
					CancelEditMode();

					// Commit the name, certain actions might need to be taken by the bound function
					OnTextCommittedDelegate.ExecuteIfBound(Text.Get(), InCommitType);
				
					return;
				}
			}
			else // When the user removes all focus from the window, revert the name
			{
				CancelEditMode();

				// Commit the name, certain actions might need to be taken by the bound function
				OnTextCommittedDelegate.ExecuteIfBound(Text.Get(), InCommitType);
				return;
			}
		}
		
		ExitEditingMode();

		OnTextCommittedDelegate.ExecuteIfBound(InText, InCommitType);

		if ( !Text.IsBound() )
		{
			TextBlock->SetText( Text );
		}
	}
}
