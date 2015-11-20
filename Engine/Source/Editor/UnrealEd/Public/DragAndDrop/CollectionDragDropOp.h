// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DecoratedDragDropOp.h"
#include "CollectionManagerTypes.h"

class FCollectionDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FCollectionDragDropOp, FDecoratedDragDropOp)

	/** Data for the collections this item represents */
	TArray<FCollectionNameType> Collections;

	static TSharedRef<FCollectionDragDropOp> New(TArray<FCollectionNameType> InCollections)
	{
		TSharedRef<FCollectionDragDropOp> Operation = MakeShareable(new FCollectionDragDropOp);
		
		Operation->MouseCursor = EMouseCursor::GrabHandClosed;
		Operation->Collections = MoveTemp(InCollections);
		Operation->Construct();

		return Operation;
	}
	
public:
	FText GetDecoratorText() const
	{
		if (CurrentHoverText.IsEmpty() && Collections.Num() > 0)
		{
			return (Collections.Num() == 1)
				? FText::FromName(Collections[0].Name)
				: FText::Format(NSLOCTEXT("ContentBrowser", "CollectionDragDropDescription", "{0} and {1} other(s)"), FText::FromName(Collections[0].Name), FText::AsNumber(Collections.Num() - 1));
		}
		
		return CurrentHoverText;
	}

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		const FSlateBrush* const CollectionIcon = (Collections.Num() > 0) ? FEditorStyle::GetBrush(ECollectionShareType::GetIconStyleName(Collections[0].Type)) : nullptr;

		return 
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ContentBrowser.AssetDragDropTooltipBackground"))
			.Content()
			[
				SNew(SHorizontalBox)

				// Left slot is collection icon
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(CollectionIcon)
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
						.Image(this, &FCollectionDragDropOp::GetIcon)
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0,0,3,0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock) 
						.Text(this, &FCollectionDragDropOp::GetDecoratorText)
					]
				]
			];
	}
};
