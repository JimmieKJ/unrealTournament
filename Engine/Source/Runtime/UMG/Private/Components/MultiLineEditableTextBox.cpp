// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "SlateFontInfo.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UMultiLineEditableTextBox

UMultiLineEditableTextBox::UMultiLineEditableTextBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ForegroundColor_DEPRECATED = FLinearColor::Black;
	BackgroundColor_DEPRECATED = FLinearColor::White;
	ReadOnlyForegroundColor_DEPRECATED = FLinearColor::Black;

	if (!IsRunningDedicatedServer())
	{
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(TEXT("/Engine/EngineFonts/Roboto"));
		Font_DEPRECATED = FSlateFontInfo(RobotoFontObj.Object, 12, FName("Bold"));
	}

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
		.Justification(Justification)
		.AutoWrapText( bAutoWrapText )
		.WrapTextAt( WrapTextAt )
//		.MinDesiredWidth(MinimumDesiredWidth)
//		.Padding(Padding)
//		.IsCaretMovedWhenGainFocus(IsCaretMovedWhenGainFocus)
//		.SelectAllTextWhenFocused(SelectAllTextWhenFocused)
//		.RevertTextOnEscape(RevertTextOnEscape)
//		.ClearKeyboardFocusOnCommit(ClearKeyboardFocusOnCommit)
//		.SelectAllTextOnCommit(SelectAllTextOnCommit)
		.OnTextChanged(BIND_UOBJECT_DELEGATE(FOnTextChanged, HandleOnTextChanged))
		.OnTextCommitted(BIND_UOBJECT_DELEGATE(FOnTextCommitted, HandleOnTextCommitted))
		;

	return MyEditableTextBlock.ToSharedRef();
}

void UMultiLineEditableTextBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	TAttribute<FText> HintTextBinding = OPTIONAL_BINDING(FText, HintText);

	MyEditableTextBlock->SetStyle(&WidgetStyle);
	MyEditableTextBlock->SetText(Text);
	MyEditableTextBlock->SetHintText(HintTextBinding);

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

	if (GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_OVERRIDES)
	{
		if (Font_DEPRECATED.HasValidFont())
		{
			WidgetStyle.Font = Font_DEPRECATED;
			Font_DEPRECATED = FSlateFontInfo();
		}

		if (ForegroundColor_DEPRECATED != FLinearColor::Black)
		{
			WidgetStyle.ForegroundColor = ForegroundColor_DEPRECATED;
			ForegroundColor_DEPRECATED = FLinearColor::Black;
		}

		if (BackgroundColor_DEPRECATED != FLinearColor::White)
		{
			WidgetStyle.BackgroundColor = BackgroundColor_DEPRECATED;
			BackgroundColor_DEPRECATED = FLinearColor::White;
		}

		if (ReadOnlyForegroundColor_DEPRECATED != FLinearColor::Black)
		{
			WidgetStyle.ReadOnlyForegroundColor = ReadOnlyForegroundColor_DEPRECATED;
			ReadOnlyForegroundColor_DEPRECATED = FLinearColor::Black;
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
