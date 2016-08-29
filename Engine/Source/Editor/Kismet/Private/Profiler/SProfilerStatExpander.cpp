// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "SProfilerStatExpander.h"

void SProfilerStatExpander::Construct( const FArguments& InArgs, const TSharedPtr<class ITableRow>& TableRow  )
{
	OwnerRowPtr = TableRow;
	StyleSet = InArgs._StyleSet;
	IndentAmount = InArgs._IndentAmount;
	BaseIndentLevel = InArgs._BaseIndentLevel;

	this->ChildSlot
	.Padding( TAttribute<FMargin>( this, &SProfilerStatExpander::GetExpanderPadding ) )
	[
		SAssignNew(ExpanderArrow, SButton)
		.ButtonStyle( FEditorStyle::Get(), "BlueprintProfiler.TreeArrowButton" )
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.Visibility( this, &SProfilerStatExpander::GetExpanderVisibility )
		.ClickMethod( EButtonClickMethod::MouseDown )
		.OnClicked( this, &SProfilerStatExpander::OnArrowClicked )
		.ContentPadding(FMargin(2.f,0.f,2.f,1.f))
		.ForegroundColor( FSlateColor::UseForeground() )
		.IsFocusable( false )
		[
			SNew(STextBlock)
			.Text( this, &SProfilerStatExpander::GetStatExpanderTextGlyph )
			.TextStyle(FEditorStyle::Get(), "BlueprintProfiler.TreeArrowGlyph")
			.ColorAndOpacity( this, &SProfilerStatExpander::GetStatExpanderColorAndOpacity )
		]
	];
}

FText SProfilerStatExpander::GetStatExpanderTextGlyph() const
{
	static const FText NormalGlyph = FText::FromString(FString(TEXT("\xf138"))); /*fa-chevron-circle-right*/
	static const FText ExpandedGlyph = FText::FromString(FString(TEXT("\xf13a"))); /*fa-chevron-circle-down*/

	return OwnerRowPtr.Pin()->IsItemExpanded() ? ExpandedGlyph : NormalGlyph;
}

FSlateColor SProfilerStatExpander::GetStatExpanderColorAndOpacity() const
{
	static const FLinearColor NormalColor(1.f, 1.f, 1.f, 0.6f);
	static const FLinearColor HoveredColor(1.f, 1.f, 1.f, 1.f);

	return ExpanderArrow->IsHovered() ? HoveredColor : NormalColor;
}

