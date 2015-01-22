// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowCentricApplication.h"

/** BT Editor public interface */
class IBehaviorTreeEditor : public FWorkflowCentricApplication
{

public:
	virtual uint32 GetSelectedNodesCount() const = 0;
	
	virtual void InitializeDebuggerState(class FBehaviorTreeDebugger* ParentDebugger) const = 0;
	virtual UEdGraphNode* FindInjectedNode(int32 Index) const = 0;
	virtual void DoubleClickNode(class UEdGraphNode* Node) = 0;
};


