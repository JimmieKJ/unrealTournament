// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsK2Node_SwitchGameplayTagContainer.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintGameplayTagLibrary.h"

UGameplayTagsK2Node_SwitchGameplayTagContainer::UGameplayTagsK2Node_SwitchGameplayTagContainer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FunctionName = TEXT("NotEqual_TagContainerTagContainer");
	FunctionClass = UBlueprintGameplayTagLibrary::StaticClass();
}

void UGameplayTagsK2Node_SwitchGameplayTagContainer::CreateFunctionPin()
{
	// Set properties on the function pin
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* FunctionPin = CreatePin(EGPD_Input, K2Schema->PC_Object, TEXT(""), FunctionClass, false, false, FunctionName.ToString());
	FunctionPin->bDefaultValueIsReadOnly = true;
	FunctionPin->bNotConnectable = true;
	FunctionPin->bHidden = true;

	UFunction* Function = FindField<UFunction>(FunctionClass, FunctionName);
	const bool bIsStaticFunc = Function->HasAllFunctionFlags(FUNC_Static);
	if (bIsStaticFunc)
	{
		// Wire up the self to the CDO of the class if it's not us
		if (UBlueprint* BP = GetBlueprint())
		{
			UClass* FunctionOwnerClass = Function->GetOuterUClass();
			if (!BP->SkeletonGeneratedClass->IsChildOf(FunctionOwnerClass))
			{
				FunctionPin->DefaultObject = FunctionOwnerClass->GetDefaultObject();
			}
		}
	}
}

void UGameplayTagsK2Node_SwitchGameplayTagContainer::PostLoad()
{
	Super::PostLoad();
	if (UEdGraphPin* FunctionPin = FindPin(FunctionName.ToString()))
	{
		FunctionPin->DefaultObject = FunctionClass->GetDefaultObject();
	}
}

void UGameplayTagsK2Node_SwitchGameplayTagContainer::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	bool bIsDirty = false;
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UGameplayTagsK2Node_SwitchGameplayTagContainer, PinContainers))
	{
		bIsDirty = true;
	}

	if (bIsDirty)
	{
		ReconstructNode();
		GetGraph()->NotifyGraphChanged();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

FText UGameplayTagsK2Node_SwitchGameplayTagContainer::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("K2Node", "Switch_TagContainer", "Switch on Gameplay Tag Container");
}

FText UGameplayTagsK2Node_SwitchGameplayTagContainer::GetTooltipText() const
{
	return NSLOCTEXT("K2Node", "SwitchTagContainer_ToolTip", "Selects an output that matches the input value");
}

void UGameplayTagsK2Node_SwitchGameplayTagContainer::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

void UGameplayTagsK2Node_SwitchGameplayTagContainer::CreateSelectionPin()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* Pin = CreatePin(EGPD_Input, K2Schema->PC_Struct, TEXT(""), FGameplayTagContainer::StaticStruct(), false, false, TEXT("Selection"));
	K2Schema->SetPinDefaultValueBasedOnType(Pin);
}

FEdGraphPinType UGameplayTagsK2Node_SwitchGameplayTagContainer::GetPinType() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	FEdGraphPinType PinType;
	PinType.PinCategory = K2Schema->PC_Struct;
	PinType.PinSubCategoryObject = FGameplayTagContainer::StaticStruct();
	return PinType;
}

FString UGameplayTagsK2Node_SwitchGameplayTagContainer::GetPinNameGivenIndex(int32 Index)
{
	check(Index);
	return PinNames[Index].ToString();
}

void UGameplayTagsK2Node_SwitchGameplayTagContainer::CreateCasePins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	for (int32 Index = 0; Index < PinNames.Num(); ++Index)
  	{
		UEdGraphPin * NewPin = CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), nullptr, false, false, PinContainers[Index].ToString());
		NewPin->PinFriendlyName = FText::FromString(PinNames[Index].ToString());
  	}
}

FString UGameplayTagsK2Node_SwitchGameplayTagContainer::GetUniquePinName()
{
	FString NewPinName;
	int32 Index = 0;
	while (true)
	{
		NewPinName = FString::Printf(TEXT("Case_%d"), Index++);
		bool bFound = false;
		for (int32 PinIdx = 0; PinIdx < Pins.Num(); PinIdx++)
		{
			FString PinName = Pins[PinIdx]->PinFriendlyName.ToString();
			if (PinName == NewPinName)
			{
				bFound = true;
				break;
			}			
		}
		if (!bFound)
		{
			break;
		}
	}
	return NewPinName;
}

void UGameplayTagsK2Node_SwitchGameplayTagContainer::AddPinToSwitchNode()
{
	FString PinName = GetUniquePinName();
	PinNames.Add(FName(*PinName));

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* NewPin = CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), nullptr, false, false, PinName);
	NewPin->PinFriendlyName = FText::FromString(PinName);
	PinContainers.Add(FGameplayTagContainer());
}

void UGameplayTagsK2Node_SwitchGameplayTagContainer::RemovePin(UEdGraphPin* TargetPin)
{
	checkSlow(TargetPin);

	// Clean-up pin name array
	PinNames.Remove(FName(*TargetPin->PinFriendlyName.ToString()));
}
