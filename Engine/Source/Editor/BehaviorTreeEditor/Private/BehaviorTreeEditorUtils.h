// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

namespace BehaviorTreeEditorUtils
{
	struct FPropertySelectionInfo
	{
		FPropertySelectionInfo()
			: FoundGraphNode_CompDecorator(nullptr)
			, FoundDecorator(nullptr)
			, bInjectedNode(false)
		{
		}

		UBehaviorTreeGraphNode_CompositeDecorator* FoundGraphNode_CompDecorator;
		UBTDecorator* FoundDecorator;
		bool bInjectedNode;
	};

	/** Given a selection of nodes, return the instances that should be selected be selected for editing in the property panel */
	TArray<UObject*> GetSelectionForPropertyEditor(const TSet<UObject*>& InSelection, FPropertySelectionInfo& OutSelectionInfo);
}
