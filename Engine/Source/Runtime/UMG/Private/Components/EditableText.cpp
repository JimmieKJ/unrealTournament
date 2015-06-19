// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "SlateFontInfo.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UEditableText

UEditableText::UEditableText(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SEditableText::FArguments Defaults;
	WidgetStyle = *Defaults._Style;

	ColorAndOpacity = FLinearColor::Black;

	static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(TEXT("/Engine/EngineFonts/Roboto"));
	Font = FSlateFontInfo(RobotoFontObj.Object, 12, FName("Bold"));

	// Grab other defaults from slate arguments.
	IsReadOnly = Defaults._IsReadOnly.Get();
	IsPassword = Defaults._IsPassword.Get();
	MinimumDesiredWidth = Defaults._MinDesiredWidth.Get();
	IsCaretMovedWhenGainFocus = Defaults._IsCaretMovedWhenGainFocus.Get();
	SelectAllTextWhenFocused = Defaults._SelectAllTextWhenFocused.Get();
	RevertTextOnEscape = Defaults._RevertTextOnEscape.Get();
	ClearKeyboardFocusOnCommit = Defaults._ClearKeyboardFocusOnCommit.Get();
	SelectAllTextOnCommit = Defaults._SelectAllTextOnCommit.Get();
}

void UEditableText::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyEditableText.Reset();
}

TSharedRef<SWidget> UEditableText::RebuildWidget()
{
	MyEditableText = SNew(SEditableText)
	.Style(&WidgetStyle)
	.Font(Font)
	.MinDesiredWidth(MinimumDesiredWidth)
	.IsCaretMovedWhenGainFocus(IsCaretMovedWhenGainFocus)
	.SelectAllTextWhenFocused(SelectAllTextWhenFocused)
	.RevertTextOnEscape(RevertTextOnEscape)
	.ClearKeyboardFocusOnCommit(ClearKeyboardFocusOnCommit)
	.SelectAllTextOnCommit(SelectAllTextOnCommit)
	.OnTextChanged(BIND_UOBJECT_DELEGATE(FOnTextChanged, HandleOnTextChanged))
	.OnTextCommitted(BIND_UOBJECT_DELEGATE(FOnTextCommitted, HandleOnTextCommitted))
	;
	
	return BuildDesignTimeWidget( MyEditableText.ToSharedRef() );
}

void UEditableText::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	TAttribute<FText> TextBinding = OPTIONAL_BINDING(FText, Text);
	TAttribute<FText> HintTextBinding = OPTIONAL_BINDING(FText, HintText);

	MyEditableText->SetText(TextBinding);
	MyEditableText->SetHintText(HintTextBinding);
	MyEditableText->SetIsReadOnly(IsReadOnly);
	MyEditableText->SetIsPassword(IsPassword);
	MyEditableText->SetColorAndOpacity(ColorAndOpacity);

	// TODO UMG Complete making all properties settable on SEditableText
}

FText UEditableText::GetText() const
{
	if ( MyEditableText.IsValid() )
	{
		return MyEditableText->GetText();
	}

	return Text;
}

void UEditableText::SetText(FText InText)
{
	Text = InText;
	if ( MyEditableText.IsValid() )
	{
		MyEditableText->SetText(Text);
	}
}

void UEditableText::SetIsPassword(bool InbIsPassword)
{
	IsPassword = InbIsPassword;
	if ( MyEditableText.IsValid() )
	{
		MyEditableText->SetIsPassword(IsPassword);
	}
}

void UEditableText::SetHintText(FText InHintText)
{
	HintText = InHintText;
	if ( MyEditableText.IsValid() )
	{
		MyEditableText->SetHintText(HintText);
	}
}

void UEditableText::SetIsReadOnly(bool InbIsReadyOnly)
{
	IsReadOnly = InbIsReadyOnly;
	if ( MyEditableText.IsValid() )
	{
		MyEditableText->SetIsReadOnly(IsReadOnly);
	}
}

void UEditableText::HandleOnTextChanged(const FText& InText)
{
	OnTextChanged.Broadcast(InText);
}

void UEditableText::HandleOnTextCommitted(const FText& InText, ETextCommit::Type CommitMethod)
{
	OnTextCommitted.Broadcast(InText, CommitMethod);
}

void UEditableText::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_ASSETS )
	{
		if ( Style_DEPRECATED != nullptr )
		{
			const FEditableTextStyle* StylePtr = Style_DEPRECATED->GetStyle<FEditableTextStyle>();
			if ( StylePtr != nullptr )
			{
				WidgetStyle = *StylePtr;
			}

			Style_DEPRECATED = nullptr;
		}

		if ( BackgroundImageSelected_DEPRECATED != nullptr )
		{
			WidgetStyle.BackgroundImageSelected = BackgroundImageSelected_DEPRECATED->Brush;
			BackgroundImageSelected_DEPRECATED = nullptr;
		}

		if ( BackgroundImageSelectionTarget_DEPRECATED != nullptr )
		{
			WidgetStyle.BackgroundImageSelectionTarget = BackgroundImageSelectionTarget_DEPRECATED->Brush;
			BackgroundImageSelectionTarget_DEPRECATED = nullptr;
		}

		if ( BackgroundImageComposing_DEPRECATED != nullptr )
		{
			WidgetStyle.BackgroundImageComposing = BackgroundImageComposing_DEPRECATED->Brush;
			BackgroundImageComposing_DEPRECATED = nullptr;
		}

		if ( CaretImage_DEPRECATED != nullptr )
		{
			WidgetStyle.CaretImage = CaretImage_DEPRECATED->Brush;
			CaretImage_DEPRECATED = nullptr;
		}
	}
}

#if WITH_EDITOR

const FSlateBrush* UEditableText::GetEditorIcon()
{
	return FUMGStyle::Get().GetBrush("Widget.EditableText");
}

const FText UEditableText::GetPaletteCategory()
{
	return LOCTEXT("Primitive", "Primitive");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
