// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "SSearchBox.h"


void SSearchBox::Construct( const FArguments& InArgs )
{
	check(InArgs._Style);

	OnSearchDelegate = InArgs._OnSearch;
	OnTextChangedDelegate = InArgs._OnTextChanged;
	OnTextCommittedDelegate = InArgs._OnTextCommitted;
	DelayChangeNotificationsWhileTyping = InArgs._DelayChangeNotificationsWhileTyping;

	InactiveFont = InArgs._Style->TextBoxStyle.Font;
	ActiveFont = InArgs._Style->ActiveFontInfo;

	bTypingFilterText = false;
	LastTypeTime = 0;
	FilterDelayAfterTyping = 0.25f;

	SEditableTextBox::Construct( SEditableTextBox::FArguments()
		.Style( &InArgs._Style->TextBoxStyle )
		.Font( this, &SSearchBox::GetWidgetFont )
		.Text( InArgs._InitialText )
		.HintText( InArgs._HintText )
		.SelectAllTextWhenFocused( InArgs._SelectAllTextWhenFocused )
		.RevertTextOnEscape( true )
		.ClearKeyboardFocusOnCommit( false )
		.OnTextChanged( this, &SSearchBox::HandleTextChanged )
		.OnTextCommitted( this, &SSearchBox::HandleTextCommitted )
		.MinDesiredWidth( InArgs._MinDesiredWidth )
	);

	// If a search delegate was bound, add a previous and next button
	if (OnSearchDelegate.IsBound())
	{
		Box->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle( FCoreStyle::Get(), "NoBorder" )
			.ContentPadding( FMargin(5, 0) )
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked( this, &SSearchBox::OnClickedSearch, SSearchBox::Previous )
			.ForegroundColor( FSlateColor::UseForeground() )
			.IsFocusable(false)
			[
				SNew(SImage)
				.Image( &InArgs._Style->UpArrowImage )
				.ColorAndOpacity( FSlateColor::UseForeground() )
			]
		];
		Box->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle( FCoreStyle::Get(), "NoBorder" )
			.ContentPadding( FMargin(5, 0) )
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked( this, &SSearchBox::OnClickedSearch, SSearchBox::Next )
			.ForegroundColor( FSlateColor::UseForeground() )
			.IsFocusable(false)
			[
				SNew(SImage)
				.Image( &InArgs._Style->DownArrowImage )
				.ColorAndOpacity( FSlateColor::UseForeground() )
			]
		];
	}

	// Add a search glass image so that the user knows this text box is for searching
	Box->AddSlot()
	.AutoWidth()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SImage)
		.Visibility(this, &SSearchBox::GetSearchGlassVisibility)
		.Image( &InArgs._Style->GlassImage )
		.ColorAndOpacity( FSlateColor::UseForeground() )
	];

	// Add an X to clear the search whenever there is some text typed into it
	Box->AddSlot()
	.AutoWidth()
	[
		SNew(SButton)
		.Visibility(this, &SSearchBox::GetXVisibility)
		.ButtonStyle( FCoreStyle::Get(), "NoBorder" )
		.ContentPadding(0)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.OnClicked( this, &SSearchBox::OnClearSearch )
		.ForegroundColor( FSlateColor::UseForeground() )
		// We want clicking on this to focus the text box instead.
		// If the user is keyboard-centric, they'll "ctrl+a, delete" to clear the search
		.IsFocusable(false)
		[
			SNew(SImage)
			.Image( &InArgs._Style->ClearImage )
			.ColorAndOpacity( FSlateColor::UseForeground() )
		]
	];
}

void SSearchBox::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	CurrentTime = InCurrentTime;

	if ( bTypingFilterText && InCurrentTime > LastTypeTime + FilterDelayAfterTyping)
{
	OnTextChangedDelegate.ExecuteIfBound( LastPendingTextChangedValue );
		bTypingFilterText = false;
	}
}

void SSearchBox::HandleTextChanged(const FText& NewText)
{
	if ( DelayChangeNotificationsWhileTyping.Get() )
	{
		LastPendingTextChangedValue = NewText;
		LastTypeTime = CurrentTime;
		bTypingFilterText = true;
	}
	else
	{
		OnTextChangedDelegate.ExecuteIfBound( NewText );
	}
}

void SSearchBox::HandleTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	bTypingFilterText = false;
	OnTextCommittedDelegate.ExecuteIfBound( NewText, CommitType );
}

EVisibility SSearchBox::GetXVisibility() const
{
	return (EditableText->GetText().IsEmpty())
		? EVisibility::Collapsed
		: EVisibility::Visible;
}

EVisibility SSearchBox::GetSearchGlassVisibility() const
{
	return (EditableText->GetText().IsEmpty())
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

FReply SSearchBox::OnClickedSearch(SSearchBox::SearchDirection Direction)
{
	OnSearchDelegate.ExecuteIfBound(Direction);
	return FReply::Handled();
}

FReply SSearchBox::OnClearSearch()
{
	// We clear the focus to commit any unset values as the search box is typically used for filtering
	// and the widget could get immediately destroyed before committing its value.
	FSlateApplication::Get().ClearKeyboardFocus( EFocusCause::SetDirectly );

	this->SetText( FText::GetEmpty() );
	return FReply::Handled().SetUserFocus(EditableText.ToSharedRef(), EFocusCause::SetDirectly);
}

FSlateFontInfo SSearchBox::GetWidgetFont() const
{
	return EditableText->GetText().IsEmpty() ? InactiveFont : ActiveFont;
}