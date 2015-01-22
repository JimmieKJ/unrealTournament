// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemEditorPrivatePCH.h"
#include "K2Node_GameplayEffectVariable.h"
#include "GameplayEffect.h"
#include "GameplayAbilityGraphSchema.h"
#include "Kismet/KismetSystemLibrary.h"
#include "BlueprintEditorUtils.h"


UGameplayAbilityGraphSchema::UGameplayAbilityGraphSchema(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

UK2Node_VariableGet* UGameplayAbilityGraphSchema::SpawnVariableGetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const
{
	// Perform handling to create custom nodes for some classes
	UProperty* VarProp = FindField<UProperty>(Source, VariableName);
	UObjectProperty* ObjProp = Cast<UObjectProperty>(VarProp);
	if (ObjProp)
	{
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph);
		TSubclassOf<UObject> GenClass = Blueprint->GeneratedClass;

		UObject* ActiveObject = GenClass->GetDefaultObject();

		// If the variable is a GameplayEffect create a custom node to show it
		FString PropType;
		ObjProp->GetCPPMacroType(PropType);
		if (PropType == "UGameplayEffect")
		{
			UK2Node_GameplayEffectVariable* NodeTemplate = NewObject<UK2Node_GameplayEffectVariable>();
			UEdGraphSchema_K2::ConfigureVarNode(NodeTemplate, VariableName, Source, Blueprint);

			UK2Node_GameplayEffectVariable* VariableNode = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_GameplayEffectVariable>(ParentGraph, NodeTemplate, GraphPosition);

			UGameplayEffect* GameplayEffect = Cast<UGameplayEffect>(ObjProp->GetObjectPropertyValue_InContainer(ActiveObject));
			if (GameplayEffect)
			{
				VariableNode->GameplayEffect = GameplayEffect;
			}

			return VariableNode;
		}
	}

	// Couldn't find an appropriate custom node for this variable, use the generic case
	return Super::SpawnVariableGetNode(GraphPosition, ParentGraph, VariableName, Source);
}

UK2Node_VariableSet* UGameplayAbilityGraphSchema::SpawnVariableSetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const
{
	return Super::SpawnVariableSetNode(GraphPosition, ParentGraph, VariableName, Source);
}