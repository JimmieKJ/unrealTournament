// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FAssetPathDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FAssetPathDragDropOp, FDragDropOperation)

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
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		FString Text = PathNames.Num() > 0 ? PathNames[0] : TEXT("");

		if ( PathNames.Num() > 1 )
		{
			Text += TEXT(" ");
			Text += FString::Printf(*NSLOCTEXT("ContentBrowser", "FolderDescription", "and %d other(s)").ToString(), PathNames.Num() - 1);
		}

		return
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush("Menu.Background") )
			.Content()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image( FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderClosed") )
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock) 
					.Text( FText::FromString(Text) )
				]
			];
	}
};
