// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "InputBindingEditorPrivatePCH.h"
#include "SGestureTreeItem.h"
#include "UICommandDragDropOp.h"




void SGestureTreeItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, TSharedPtr<FGestureTreeItem> InItem)
{
	TreeItem = InItem;

	SMultiColumnTableRow< TSharedPtr<FGestureTreeItem> >::Construct(FSuperRowType::FArguments(), InOwnerTable);
}

TSharedRef<SWidget> SGestureTreeItem::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == "Name")
	{
		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SExpanderArrow, SharedThis(this))
			]

		+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(TreeItem->CommandInfo->GetLabel())
					.ToolTipText(TreeItem->CommandInfo->GetDescription())
				]

				+ SVerticalBox::Slot()
					.Padding(0.0f, 0.0f, 0.0f, 5.0f)
					.AutoHeight()
					[
						SNew(STextBlock)
						.Font(FEditorStyle::GetFontStyle("InputBindingEditor.SmallFont"))
						.ColorAndOpacity(FLinearColor::Gray)
						.Text(TreeItem->CommandInfo->GetDescription())
					]
			];
	}
	else if (ColumnName == "Binding")
	{
		return SNew(SBox)
			.Padding(2.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SGestureEditBox, TreeItem)
			];
	}
	else
	{
		return SNew(STextBlock).Text(NSLOCTEXT("GestureTreeItem", "UnknownColumn", "Unknown Column"));
	}
}

FReply SGestureTreeItem::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && FMultiBoxSettings::IsInToolbarEditMode())
	{
		return FReply::Handled().DetectDrag(SharedThis(this), MouseEvent.GetEffectingButton());
	}

	return FReply::Unhandled();
}

FReply SGestureTreeItem::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FToolBarBuilder TempBuilder(MakeShareable(new FUICommandList), FMultiBoxCustomization::None);
	TempBuilder.AddToolBarButton(TreeItem->CommandInfo.ToSharedRef());

	TSharedRef<SWidget> CursorDecorator = TempBuilder.MakeWidget();

	return FReply::Handled().BeginDragDrop(FUICommandDragDropOp::New(TreeItem->CommandInfo.ToSharedRef(), NAME_None, TempBuilder.MakeWidget(), FVector2D::ZeroVector));
}
