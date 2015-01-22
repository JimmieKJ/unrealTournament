// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "AnimGraphNode_WheelHandler.h"
#include "CompilerResultsLog.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_WheelHandler

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_WheelHandler::UAnimGraphNode_WheelHandler(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_WheelHandler::GetControllerDescription() const
{
	return LOCTEXT("AnimGraphNode_WheelHandler", "Wheel Handler for WheeledVehicle");
}

FString UAnimGraphNode_WheelHandler::GetKeywords() const
{
	return TEXT("Modify, Wheel, Vehicle");
}

FText UAnimGraphNode_WheelHandler::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_WheelHandler_Tooltip", "This alters the wheel transform based on set up in Wheeled Vehicle. This only works when the owner is WheeledVehicle.");
}

FText UAnimGraphNode_WheelHandler::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FText NodeTitle;
	if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
	{
		NodeTitle = GetControllerDescription();
	}
	else
	{
		// we don't have any run-time information, so it's limited to print  
		// anymore than what it is it would be nice to print more data such as 
		// name of bones for wheels, but it's not available in Persona
		NodeTitle = FText(LOCTEXT("AnimGraphNode_WheelHandler_Title", "Wheel Handler"));
	}	
	return NodeTitle;
}

void UAnimGraphNode_WheelHandler::ValidateAnimNodePostCompile(class FCompilerResultsLog& MessageLog, class UAnimBlueprintGeneratedClass* CompiledClass, int32 CompiledNodeIndex)
{
	// we only support vehicle anim instance
	if ( CompiledClass->IsChildOf(UVehicleAnimInstance::StaticClass())  == false )
	{
		MessageLog.Error(TEXT("@@ is only allowwed in VehicleAnimInstance. If this is for vehicle, please change parent to be VehicleAnimInstancen (Reparent Class)."), this);
	}
}

void UAnimGraphNode_WheelHandler::GetMenuEntries(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	UAnimBlueprint* Blueprint = Cast<UAnimBlueprint> (FBlueprintEditorUtils::FindBlueprintForGraph(ContextMenuBuilder.CurrentGraph));

	if (Blueprint &&  Blueprint->ParentClass->IsChildOf(UVehicleAnimInstance::StaticClass()))
	{
		Super::GetMenuEntries( ContextMenuBuilder );
	}

	// else we don't show
}

bool UAnimGraphNode_WheelHandler::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	return (Blueprint != nullptr) && Blueprint->ParentClass->IsChildOf<UVehicleAnimInstance>() && Super::IsCompatibleWithGraph(TargetGraph);
}

#undef LOCTEXT_NAMESPACE
