// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "MultiBox.h"
#include "SWidgetBlock.h"


/**
 * Constructor
 *
 * @param	InHeadingText	Heading text
 */
FWidgetBlock::FWidgetBlock( TSharedRef<SWidget> InContent, const FText& InLabel, bool bInNoIndent )
	: FMultiBlock( nullptr, nullptr, NAME_None, EMultiBlockType::Widget )
	, ContentWidget( InContent )
	, Label( InLabel )
	, bNoIndent( bInNoIndent )
{
}


void FWidgetBlock::CreateMenuEntry(FMenuBuilder& MenuBuilder) const
{
	FText EntryLabel = (!Label.IsEmpty()) ? Label : NSLOCTEXT("WidgetBlock", "CustomControl", "Custom Control");
	MenuBuilder.AddWidget(ContentWidget, FText::GetEmpty(), true);
}


/**
 * Allocates a widget for this type of MultiBlock.  Override this in derived classes.
 *
 * @return  MultiBlock widget object
 */
TSharedRef< class IMultiBlockBaseWidget > FWidgetBlock::ConstructWidget() const
{
	return SNew( SWidgetBlock )
			.Cursor(EMouseCursor::Default);
}


/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SWidgetBlock::Construct( const FArguments& InArgs )
{
}



/**
 * Builds this MultiBlock widget up from the MultiBlock associated with it
 */
void SWidgetBlock::BuildMultiBlockWidget(const ISlateStyle* StyleSet, const FName& StyleName)
{
	TSharedRef< const FWidgetBlock > WidgetBlock = StaticCastSharedRef< const FWidgetBlock >( MultiBlock.ToSharedRef() );

	bool bHasLabel = !WidgetBlock->Label.IsEmpty();
	FMargin Padding = WidgetBlock->bNoIndent ? StyleSet->GetMargin( StyleName, ".Block.Padding" ) : StyleSet->GetMargin( StyleName, ".Block.IndentedPadding" );

	ChildSlot
	.Padding( Padding )	// Large left margin mimics the indent of normal menu items when bNoIndent is false
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SHorizontalBox)
			.Visibility( bHasLabel ? EVisibility::Visible : EVisibility::Collapsed )
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.f, 0.f, 4.f, 0.f)
			.VAlign( VAlign_Center )
			[
				SNew( STextBlock )
				.TextStyle( StyleSet, ISlateStyle::Join( StyleName, ".Label" ) )
				.Text( WidgetBlock->Label )
			]
		]
		+SHorizontalBox::Slot()
		.VAlign( VAlign_Bottom )
		.FillWidth(1.f)
		[
			WidgetBlock->ContentWidget
		]
	];
}
