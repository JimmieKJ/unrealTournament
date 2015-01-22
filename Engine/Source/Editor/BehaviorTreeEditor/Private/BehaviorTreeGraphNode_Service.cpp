// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTreeEditorPrivatePCH.h"
#include "BehaviorTree/BTService.h"

UBehaviorTreeGraphNode_Service::UBehaviorTreeGraphNode_Service(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBehaviorTreeGraphNode_Service::AllocateDefaultPins()
{
	//No Pins for services
}


FText UBehaviorTreeGraphNode_Service::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const UBTService* Service = Cast<UBTService>(NodeInstance);
	if (Service != NULL)
	{
		return FText::FromString(Service->GetNodeName());
	}
	else if (!ClassData.GetClassName().IsEmpty())
	{
		return NSLOCTEXT("BehaviorTreeGraphNode", "UnknownNodeClass", "Can't load class!");
	}

	return Super::GetNodeTitle(TitleType);
}

bool UBehaviorTreeGraphNode_Service::IsSubNode() const
{
	return true;
}
