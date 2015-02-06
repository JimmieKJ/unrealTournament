// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "DragAndDrop.h"
#include "Engine/LevelStreaming.h"

class FLevelDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FLevelDragDropOp, FDragDropOperation)

	/** The levels to be dropped. */
	TArray<TWeakObjectPtr<ULevel>> LevelsToDrop;
		
	/** The streaming levels to be dropped. */
	TArray<TWeakObjectPtr<ULevelStreaming>> StreamingLevelsToDrop;

	/** Whether content is good to drop on current site, used by decorator */
	bool bGoodToDrop;
		
	/** The widget decorator to use */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		FString LevelName(TEXT("None"));
		
		if (LevelsToDrop.Num())
		{
			if (LevelsToDrop[0].IsValid())
			{
				LevelName = LevelsToDrop[0]->GetOutermost()->GetName();
			}
		}
		else if (StreamingLevelsToDrop.Num())
		{
			if (StreamingLevelsToDrop[0].IsValid())
			{
				LevelName = StreamingLevelsToDrop[0]->GetWorldAssetPackageName();
			}
		}
		
		return SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("Graph.ConnectorFeedback.Border"))
			.Content()
			[			
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
					[
						SNew(STextBlock) 
						.Text( FText::FromString(LevelName) )
					]
			];
	}

	static TSharedRef<FLevelDragDropOp> New(const TArray<TWeakObjectPtr<ULevelStreaming>>& LevelsToDrop)
	{
		TSharedRef<FLevelDragDropOp> Operation = MakeShareable(new FLevelDragDropOp);
		Operation->StreamingLevelsToDrop.Append(LevelsToDrop);
		Operation->bGoodToDrop = true;
		Operation->Construct();
		return Operation;
	}

	static TSharedRef<FLevelDragDropOp> New(const TArray<TWeakObjectPtr<ULevel>>& LevelsToDrop)
	{
		TSharedRef<FLevelDragDropOp> Operation = MakeShareable(new FLevelDragDropOp);
		Operation->LevelsToDrop.Append(LevelsToDrop);
		Operation->bGoodToDrop = true;
		Operation->Construct();
		return Operation;
	}
};
