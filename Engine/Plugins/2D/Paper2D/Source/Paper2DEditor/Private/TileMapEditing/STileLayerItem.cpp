// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "STileLayerItem.h"
#include "SContentReference.h"
#include "ScopedTransaction.h"
#include "SInlineEditableTextBlock.h"
#include "PaperStyle.h"

#define LOCTEXT_NAMESPACE "Paper2D"

//////////////////////////////////////////////////////////////////////////
// STileLayerItem

void STileLayerItem::Construct(const FArguments& InArgs, class UPaperTileLayer* InItem, FIsSelected InIsSelectedDelegate)
{
	MyLayer = InItem;

	static const FName EyeClosedBrushName("TileMapEditor.LayerEyeClosed");
	static const FName EyeOpenedBrushName("TileMapEditor.LayerEyeOpened");
	EyeClosed = FPaperStyle::Get()->GetBrush(EyeClosedBrushName);
	EyeOpened = FPaperStyle::Get()->GetBrush(EyeOpenedBrushName);

	ChildSlot
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SAssignNew( VisibilityButton, SButton )
			.ContentPadding(FMargin(4.0f, 4.0f, 4.0f, 4.0f))
			.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
			.OnClicked( this, &STileLayerItem::OnToggleVisibility )
			.ToolTipText( LOCTEXT("LayerVisibilityButtonToolTip", "Toggle Layer Visibility") )
			.ForegroundColor( FSlateColor::UseForeground() )
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Center )
			.Content()
			[
				SNew(SImage)
				.Image(this, &STileLayerItem::GetVisibilityBrushForLayer)
				.ColorAndOpacity(this, &STileLayerItem::GetForegroundColorForVisibilityButton)
			]
		]
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(FMargin(4.0f, 4.0f, 4.0f, 4.0f))
		[
			SNew(SInlineEditableTextBlock)
			.Text(this, &STileLayerItem::GetLayerDisplayName)
			.ToolTipText(this, &STileLayerItem::GetLayerDisplayName)
			.OnTextCommitted(this, &STileLayerItem::OnLayerNameCommitted)
			.IsSelected(InIsSelectedDelegate)
		]
	];
}

FText STileLayerItem::GetLayerDisplayName() const
{
	const FText UnnamedText = LOCTEXT("NoLayerName", "(unnamed)");

	return MyLayer->LayerName.IsEmpty() ? UnnamedText : MyLayer->LayerName;
}

void STileLayerItem::OnLayerNameCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	const FScopedTransaction Transaction( LOCTEXT("TileMapRenameLayer", "Rename Layer") );
	MyLayer->SetFlags(RF_Transactional);
	MyLayer->Modify();
	MyLayer->LayerName = NewText;
}

FReply STileLayerItem::OnToggleVisibility()
{
	const FScopedTransaction Transaction( LOCTEXT("ToggleVisibility", "Toggle Layer Visibility") );
	MyLayer->SetFlags(RF_Transactional);
	MyLayer->Modify();
	MyLayer->bHiddenInEditor = !MyLayer->bHiddenInEditor;
	MyLayer->PostEditChange();
	return FReply::Handled();
}

const FSlateBrush* STileLayerItem::GetVisibilityBrushForLayer() const
{
	return MyLayer->bHiddenInEditor ? EyeClosed : EyeOpened;
}

FSlateColor STileLayerItem::GetForegroundColorForVisibilityButton() const
{
	static const FName InvertedForeground("InvertedForeground");
	return FEditorStyle::GetSlateColor(InvertedForeground);
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE