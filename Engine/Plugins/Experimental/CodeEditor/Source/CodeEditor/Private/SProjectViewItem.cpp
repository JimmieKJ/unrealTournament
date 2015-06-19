// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CodeEditorPrivatePCH.h"
#include "SProjectViewItem.h"


#define LOCTEXT_NAMESPACE "ProjectViewItem"


void SProjectViewItem::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(1.0f)
		.AutoWidth()
		[
			SNew(SImage)
			.Image(FCodeEditorStyle::Get().GetBrush(InArgs._IconName))
		]
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(1.0f)
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text(InArgs._Text)
		]
	];
}


#undef LOCTEXT_NAMESPACE
