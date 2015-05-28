// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#include "RichTextBlockDecorator.h"
#include "RichTextBlock.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// URichTextBlock

URichTextBlock::URichTextBlock(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(TEXT("/Engine/EngineFonts/Roboto"));
	Font = FSlateFontInfo(RobotoFontObj.Object, 12, FName("Regular"));
	Color = FLinearColor::White;
	LineHeightPercentage = 1;

	Decorators.Add(ObjectInitializer.CreateOptionalDefaultSubobject<URichTextBlockDecorator>(this, FName("DefaultDecorator")));
}

void URichTextBlock::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyRichTextBlock.Reset();
}

TSharedRef<SWidget> URichTextBlock::RebuildWidget()
{
	//+ OnHyperlinkClicked = FSlateHyperlinkRun::FOnClick::CreateStatic(&RichTextHelper::OnBrowserLinkClicked, AsShared());
	//+ FHyperlinkDecorator::Create(TEXT("browser"), OnHyperlinkClicked))
	//+MakeShareable(new FDefaultRichTextDecorator(Font, Color));

	DefaultStyle.SetFont(Font);
	DefaultStyle.SetColorAndOpacity(Color);

	TArray< TSharedRef< class ITextDecorator > > CreatedDecorators;

	for ( URichTextBlockDecorator* Decorator : Decorators )
	{
		if ( Decorator )
		{
			CreatedDecorators.Add(Decorator->CreateDecorator(Font, Color));
		}
	}

	MyRichTextBlock =
		SNew(SRichTextBlock)
		.Justification(Justification)
		.AutoWrapText(AutoWrapText)
		.WrapTextAt(WrapTextAt)
		.Margin(Margin)
		.LineHeightPercentage(LineHeightPercentage)
		.TextStyle(&DefaultStyle)
		.Decorators(CreatedDecorators);
	
	return MyRichTextBlock.ToSharedRef();
}

void URichTextBlock::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	TAttribute<FText> TextBinding = OPTIONAL_BINDING(FText, Text);

	MyRichTextBlock->SetText(TextBinding);
}

#if WITH_EDITOR

const FSlateBrush* URichTextBlock::GetEditorIcon()
{
	return FUMGStyle::Get().GetBrush("Widget.RichTextBlock");
}

const FText URichTextBlock::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
