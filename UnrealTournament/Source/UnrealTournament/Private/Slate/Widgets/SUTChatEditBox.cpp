// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTEditableTextBox.h"
#include "SUTChatEditBox.h"

#if !UE_SERVER

void SUTChatEditBox::Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> inPlayerOwner)
{

	MaxTextSize = InArgs._MaxTextSize;
	PlayerOwner = inPlayerOwner;

	ExternalOnTextChanged = InArgs._OnTextChanged;
	ExternalOnTextCommitted = InArgs._OnTextCommitted;

	check (InArgs._Style);
	SetStyle(InArgs._Style);

	FontOverride = InArgs._Font;
	ForegroundColorOverride = InArgs._ForegroundColor;
	BackgroundColorOverride = InArgs._BackgroundColor;
	TAttribute<FMargin> Padding = InArgs._Style->Padding;
	TAttribute<FSlateFontInfo> Font = FontOverride.IsSet() ? FontOverride : InArgs._Style->Font;
	TAttribute<FSlateColor> BorderForegroundColor = ForegroundColorOverride.IsSet() ? ForegroundColorOverride : InArgs._Style->ForegroundColor;
	TAttribute<FSlateColor> BackgroundColor = BackgroundColorOverride.IsSet() ? BackgroundColorOverride : InArgs._Style->BackgroundColor;
	ReadOnlyForegroundColor = InArgs._Style->ReadOnlyForegroundColor;

	SBorder::Construct( SBorder::FArguments()
		.BorderImage( this, &SUTChatEditBox::GetBorderImage )
		.BorderBackgroundColor( BackgroundColor )
		.ForegroundColor( BorderForegroundColor )
		.Padding( 0 )
		[
			SAssignNew( Box, SHorizontalBox)

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.FillWidth(1)
			[
				SAssignNew(PaddingBox, SBox)
				.Padding( Padding )
				[
					SNew(SOverlay)
					+SOverlay::Slot()
					[
						SAssignNew( EditableText, SEditableText )
						.HintText( InArgs._HintText )
						.Font( Font )
						.IsReadOnly( false )
						.IsPassword( false )
						.IsCaretMovedWhenGainFocus( false )
						.SelectAllTextWhenFocused( false )
						.RevertTextOnEscape( false )
						.ClearKeyboardFocusOnCommit( false )
						.OnTextChanged( this, &SUTChatEditBox::EditableTextChanged)
						.OnTextCommitted( this, &SUTChatEditBox::EditableTextCommited)
						.MinDesiredWidth( InArgs._MinDesiredWidth )
						.SelectAllTextOnCommit( false )
						.OnKeyDownHandler( this, &SUTChatEditBox::InternalOnKeyDown ) 
					]
					+SOverlay::Slot()
					[
						SAssignNew(TypeMsg, STextBlock)
						.Text(FText::FromString(TEXT("type your message here")))
						.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
						.Visibility(this, &SUTChatEditBox::TypedMsgVis)
						.ColorAndOpacity(FLinearColor(1.0,1.0,1.0,0.3))
					]
				]
			]
		]
	);
}

void SUTChatEditBox::SetChangedDelegate(FOnTextChanged inOnTextChanged)
{
	ExternalOnTextChanged = inOnTextChanged;
}
void SUTChatEditBox::SetCommittedDelegate(FOnTextCommitted inOnTextCommitted)
{
	ExternalOnTextCommitted = inOnTextCommitted;
}


FReply SUTChatEditBox::InternalOnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{

	if (InKeyEvent.GetKey() == EKeys::Up || InKeyEvent.GetKey() == EKeys::Down)
	{
		FText NewText = PlayerOwner->GetUIChatTextBackBuffer((InKeyEvent.GetKey() == EKeys::Up) ? -1 : 1);
		if (!NewText.IsEmpty())
		{
			EditableText->SetText(NewText);
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

void SUTChatEditBox::EditableTextChanged(const FText& NewText)
{
	// If we have to crop the text, crop it.  The SetText call will recuse back so just exit afterwards.
	if (NewText.ToString().Len() > MaxTextSize)
	{
		FText Cropped = FText::FromString( NewText.ToString().Left(128));
		SetText(Cropped);
		return;
	}

	ExternalOnTextChanged.ExecuteIfBound(NewText);
	if (PlayerOwner.IsValid())
	{
		PlayerOwner->UIChatTextBuffer = NewText;
	}
}

void SUTChatEditBox::EditableTextCommited(const FText& NewText, ETextCommit::Type CommitType)
{
	PlayerOwner->UpdateUIChatTextBackBuffer(NewText);
	ExternalOnTextCommitted.ExecuteIfBound(NewText, CommitType);
}

EVisibility SUTChatEditBox::TypedMsgVis() const
{
	return (EditableText->GetText().IsEmpty()) ? EVisibility::Visible : EVisibility::Hidden;
}


const FSlateBrush* SUTChatEditBox::GetBorderImage() const
{
	if ( EditableText->IsTextReadOnly() )
	{
		return &Style->BackgroundImageReadOnly;
	}
	else if ( EditableText->HasKeyboardFocus() )
	{
		return &Style->BackgroundImageFocused;
	}
	else
	{
		if ( EditableText->IsHovered() )
		{
			return &Style->BackgroundImageHovered;
		}
		else
		{
			return &Style->BackgroundImageNormal;
		}
	}
}

FReply SUTChatEditBox::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InKeyboardFocusEvent )
{
	JumpToEnd();
	return (EditableText.IsValid()) ? FReply::Handled().SetUserFocus(EditableText.ToSharedRef(),EFocusCause::SetDirectly) : FReply::Unhandled();
}


#endif