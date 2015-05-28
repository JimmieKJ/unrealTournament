// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "SlateFontInfo.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UMultiLineEditableTextBox

UMultiLineEditableTextBox::UMultiLineEditableTextBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ForegroundColor = FLinearColor::Black;
	BackgroundColor = FLinearColor::White;
	ReadOnlyForegroundColor = FLinearColor::Black;

	static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(TEXT("/Engine/EngineFonts/Roboto"));
	Font = FSlateFontInfo(RobotoFontObj.Object, 12, FName("Bold"));

	bAutoWrapText = true;

	SMultiLineEditableTextBox::FArguments Defaults;
	WidgetStyle = *Defaults._Style;
	TextStyle = *Defaults._TextStyle;
}

void UMultiLineEditableTextBox::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyEditableTextBlock.Reset();
}

TSharedRef<SWidget> UMultiLineEditableTextBox::RebuildWidget()
{
	MyEditableTextBlock = SNew(SMultiLineEditableTextBox)
		.Style(&WidgetStyle)
		.TextStyle(&TextStyle)
		.Font(Font)
		.Justification(Justification)
		.ForegroundColor(ForegroundColor)
		.BackgroundColor(BackgroundColor)
		.ReadOnlyForegroundColor(ReadOnlyForegroundColor)
		.AutoWrapText( bAutoWrapText )
		.WrapTextAt( WrapTextAt )
//		.MinDesiredWidth(MinimumDesiredWidth)
//		.Padding(Padding)
//		.IsCaretMovedWhenGainFocus(IsCaretMovedWhenGainFocus)
//		.SelectAllTextWhenFocused(SelectAllTextWhenFocused)
//		.RevertTextOnEscape(RevertTextOnEscape)
//		.ClearKeyboardFocusOnCommit(ClearKeyboardFocusOnCommit)
//		.SelectAllTextOnCommit(SelectAllTextOnCommit)
//		.OnTextChanged(BIND_UOBJECT_DELEGATE(FOnTextChanged, HandleOnTextChanged))
//		.OnTextCommitted(BIND_UOBJECT_DELEGATE(FOnTextCommitted, HandleOnTextCommitted))
		;

	return MyEditableTextBlock.ToSharedRef();
}

void UMultiLineEditableTextBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyEditableTextBlock->SetText(Text);
//	MyEditableTextBlock->SetHintText(HintText);
//	MyEditableTextBlock->SetIsReadOnly(IsReadOnly);
//	MyEditableTextBlock->SetIsPassword(IsPassword);
//	MyEditableTextBlock->SetColorAndOpacity(ColorAndOpacity);

	// TODO UMG Complete making all properties settable on SMultiLineEditableTextBox
}

FText UMultiLineEditableTextBox::GetText() const
{
	if ( MyEditableTextBlock.IsValid() )
	{
		return MyEditableTextBlock->GetText();
	}

	return Text;
}

void UMultiLineEditableTextBox::SetText(FText InText)
{
	Text = InText;
	if ( MyEditableTextBlock.IsValid() )
	{
		MyEditableTextBlock->SetText(Text);
	}
}

void UMultiLineEditableTextBox::SetError(FText InError)
{
	if ( MyEditableTextBlock.IsValid() )
	{
		MyEditableTextBlock->SetError(InError);
	}
}

void UMultiLineEditableTextBox::HandleOnTextChanged(const FText& InText)
{
	OnTextChanged.Broadcast(InText);
}

void UMultiLineEditableTextBox::HandleOnTextCommitted(const FText& InText, ETextCommit::Type CommitMethod)
{
	OnTextCommitted.Broadcast(InText, CommitMethod);
}

void UMultiLineEditableTextBox::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_ASSETS )
	{
		if ( Style_DEPRECATED != nullptr )
		{
			const FEditableTextBoxStyle* StylePtr = Style_DEPRECATED->GetStyle<FEditableTextBoxStyle>();
			if ( StylePtr != nullptr )
			{
				WidgetStyle = *StylePtr;
			}

			Style_DEPRECATED = nullptr;
		}
	}
}

#if WITH_EDITOR

const FSlateBrush* UMultiLineEditableTextBox::GetEditorIcon()
{
	return FUMGStyle::Get().GetBrush("Widget.MultiLineEditableTextBox");
}

const FText UMultiLineEditableTextBox::GetPaletteCategory()
{
	return LOCTEXT("Input", "Input");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
