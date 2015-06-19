// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UMultiLineEditableText

UMultiLineEditableText::UMultiLineEditableText(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SMultiLineEditableText::FArguments Defaults;
	WidgetStyle = *Defaults._TextStyle;

	bAutoWrapText = true;
	
	static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(TEXT("/Engine/EngineFonts/Roboto"));
	Font = FSlateFontInfo(RobotoFontObj.Object, 12, FName("Bold"));
}

void UMultiLineEditableText::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyMultiLineEditableText.Reset();
}

TSharedRef<SWidget> UMultiLineEditableText::RebuildWidget()
{
	MyMultiLineEditableText = SNew(SMultiLineEditableText)
	.TextStyle(&WidgetStyle)
	.Font(Font)
	.Justification(Justification)
	.WrapTextAt( WrapTextAt )
	.AutoWrapText( bAutoWrapText )
//	.MinDesiredWidth(MinimumDesiredWidth)
//	.IsCaretMovedWhenGainFocus(IsCaretMovedWhenGainFocus)
//	.SelectAllTextWhenFocused(SelectAllTextWhenFocused)
//	.RevertTextOnEscape(RevertTextOnEscape)
//	.ClearKeyboardFocusOnCommit(ClearKeyboardFocusOnCommit)
//	.SelectAllTextOnCommit(SelectAllTextOnCommit)
//	.BackgroundImageSelected(BackgroundImageSelected ? TAttribute<const FSlateBrush*>(&BackgroundImageSelected->Brush) : TAttribute<const FSlateBrush*>())
//	.BackgroundImageSelectionTarget(BackgroundImageSelectionTarget ? TAttribute<const FSlateBrush*>(&BackgroundImageSelectionTarget->Brush) : TAttribute<const FSlateBrush*>())
//	.BackgroundImageComposing(BackgroundImageComposing ? TAttribute<const FSlateBrush*>(&BackgroundImageComposing->Brush) : TAttribute<const FSlateBrush*>())
//	.CaretImage(CaretImage ? TAttribute<const FSlateBrush*>(&CaretImage->Brush) : TAttribute<const FSlateBrush*>())
//	.OnTextChanged(BIND_UOBJECT_DELEGATE(FOnTextChanged, HandleOnTextChanged))
//	.OnTextCommitted(BIND_UOBJECT_DELEGATE(FOnTextCommitted, HandleOnTextCommitted))
	;
	
	return BuildDesignTimeWidget( MyMultiLineEditableText.ToSharedRef() );
}

void UMultiLineEditableText::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyMultiLineEditableText->SetText(Text);
//	MyMultiLineEditableText->SetHintText(HintText);
//	MyMultiLineEditableText->SetIsReadOnly(IsReadOnly);
//	MyMultiLineEditableText->SetIsPassword(IsPassword);
//	MyMultiLineEditableText->SetColorAndOpacity(ColorAndOpacity);

	// TODO UMG Complete making all properties settable on SMultiLineEditableText
}

FText UMultiLineEditableText::GetText() const
{
	if ( MyMultiLineEditableText.IsValid() )
	{
		return MyMultiLineEditableText->GetText();
	}

	return Text;
}

void UMultiLineEditableText::SetText(FText InText)
{
	Text = InText;
	if ( MyMultiLineEditableText.IsValid() )
	{
		MyMultiLineEditableText->SetText(Text);
	}
}

void UMultiLineEditableText::HandleOnTextChanged(const FText& InText)
{
	OnTextChanged.Broadcast(InText);
}

void UMultiLineEditableText::HandleOnTextCommitted(const FText& InText, ETextCommit::Type CommitMethod)
{
	OnTextCommitted.Broadcast(InText, CommitMethod);
}

#if WITH_EDITOR

const FSlateBrush* UMultiLineEditableText::GetEditorIcon()
{
	return FUMGStyle::Get().GetBrush("Widget.MultiLineEditableText");
}

const FText UMultiLineEditableText::GetPaletteCategory()
{
	return LOCTEXT("Primitive", "Primitive");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
