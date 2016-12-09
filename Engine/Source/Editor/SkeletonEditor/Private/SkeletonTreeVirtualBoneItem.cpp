// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SkeletonTreeVirtualBoneItem.h"
#include "Widgets/Text/STextBlock.h"
#include "SSkeletonTreeRow.h"
#include "IPersonaPreviewScene.h"
#include "EditorStyleSet.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "FSkeletonTreeVirtualBoneItem"

TSharedRef<ITableRow> FSkeletonTreeVirtualBoneItem::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, const TAttribute<FText>& InFilterText)
{
	return
		SNew(SSkeletonTreeRow, InOwnerTable)
		.Item(SharedThis(this))
		.FilterText(InFilterText);
}

EVisibility FSkeletonTreeVirtualBoneItem::GetLODIconVisibility() const
{
	return EVisibility::Visible;
}

void FSkeletonTreeVirtualBoneItem::GenerateWidgetForNameColumn(TSharedPtr< SHorizontalBox > Box, const TAttribute<FText>& FilterText, FIsSelected InIsSelected)
{
	const FSlateBrush* LODIcon = FEditorStyle::GetBrush("SkeletonTree.LODBone");

	Box->AddSlot()
		.AutoWidth()
		.Padding(FMargin(0.0f, 1.0f))
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
			.Image(LODIcon)
			.Visibility(this, &FSkeletonTreeVirtualBoneItem::GetLODIconVisibility)
		];

	FText ToolTip = GetBoneToolTip();
	Box->AddSlot()
		.AutoWidth()
		.Padding(2, 0, 0, 0)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.ColorAndOpacity(this, &FSkeletonTreeVirtualBoneItem::GetBoneTextColor)
			.Text(FText::FromName(BoneName))
			.HighlightText(FilterText)
			.Font(this, &FSkeletonTreeVirtualBoneItem::GetBoneTextFont)
			.ToolTipText(ToolTip)
		];
}

TSharedRef< SWidget > FSkeletonTreeVirtualBoneItem::GenerateWidgetForDataColumn(const FName& DataColumnName)
{
	return SNullWidget::NullWidget;
}

FSlateFontInfo FSkeletonTreeVirtualBoneItem::GetBoneTextFont() const
{
	return FEditorStyle::GetWidgetStyle<FTextBlockStyle>("SkeletonTree.BoldFont").Font;
}

FSlateColor FSkeletonTreeVirtualBoneItem::GetBoneTextColor() const
{
	return FSlateColor(FLinearColor(0.4f, 0.4f, 1.f));
}

FText FSkeletonTreeVirtualBoneItem::GetBoneToolTip()
{
	return LOCTEXT("VirtualBone_ToolTip", "Virtual Bones are added in editor and allow space switching between two different bones in the skeleton.");
}

#undef LOCTEXT_NAMESPACE
