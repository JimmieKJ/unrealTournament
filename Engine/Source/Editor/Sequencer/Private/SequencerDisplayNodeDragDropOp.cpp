// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequencerDisplayNodeDragDropOp.h"


FSequencerDisplayNodeDragDropOp::FSequencerDisplayNodeDragDropOp( TArray<TSharedRef<FSequencerDisplayNode>>& InDraggedNodes )
{
	DraggedNodes = InDraggedNodes;
}


TArray<TSharedRef<FSequencerDisplayNode>>& FSequencerDisplayNodeDragDropOp::GetDraggedNodes()
{
	return DraggedNodes;
}
