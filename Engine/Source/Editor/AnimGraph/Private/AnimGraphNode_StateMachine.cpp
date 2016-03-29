// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "AnimGraphNode_StateMachine.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_StateMachine

UAnimGraphNode_StateMachine::UAnimGraphNode_StateMachine(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCanRenameNode = true;
}
