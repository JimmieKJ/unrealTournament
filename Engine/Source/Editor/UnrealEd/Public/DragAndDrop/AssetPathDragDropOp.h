// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DecoratedDragDropOp.h"

class FAssetPathDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FAssetPathDragDropOp, FDecoratedDragDropOp)

	/** Data for the asset this item represents */
	TArray<FString> PathNames;

	static TSharedRef<FAssetPathDragDropOp> New(const TArray<FString>& InPathNames)
	{
		TSharedRef<FAssetPathDragDropOp> Operation = MakeShareable(new FAssetPathDragDropOp);
		
		Operation->MouseCursor = EMouseCursor::GrabHandClosed;
		Operation->PathNames = InPathNames;
		Operation->Construct();

		return Operation;
	}
	
public:
	FText GetDecoratorText() const
	{
		if ( CurrentHoverText.IsEmpty() )
		{
			FString Text = PathNames.Num() > 0 ? PathNames[0] : TEXT("");

			if ( PathNames.Num() > 1 )
			{
				Text += TEXT(" ");
				Text += FString::Printf(*NSLOCTEXT("ContentBrowser", "FolderDescription", "and %d other(s)").ToString(), PathNames.Num() - 1);
			}

			return FText::FromString(Text);
		}
		
		return CurrentHoverText;
	}

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		return 
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ContentBrowser.AssetDragDropTooltipBackground"))
			.Content()
			[
				SNew(SHorizontalBox)

				// Left slot is folder icon
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderClosed"))
				]

				// Right slot is for tooltip
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(3.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SImage) 
						.Image(this, &FAssetPathDragDropOp::GetIcon)
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0,0,3,0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock) 
						.Text(this, &FAssetPathDragDropOp::GetDecoratorText)
					]
				]
			];
	}
};
