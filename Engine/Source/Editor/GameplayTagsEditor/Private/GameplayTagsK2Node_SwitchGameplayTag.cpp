// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsEditorModulePrivatePCH.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsK2Node_SwitchGameplayTag.h"

UGameplayTagsK2Node_SwitchGameplayTag::UGameplayTagsK2Node_SwitchGameplayTag(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FunctionName = TEXT("NotEqual_TagTag");
	FunctionClass = UGameplayTagsK2Node_SwitchGameplayTag::StaticClass();
}

void UGameplayTagsK2Node_SwitchGameplayTag::CreateFunctionPin()
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

bool UGameplayTagsK2Node_SwitchGameplayTag::NotEqual_TagTag(FGameplayTag A, FString B)
{
	return A.ToString() != B;
}

void UGameplayTagsK2Node_SwitchGameplayTag::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	bool bIsDirty = false;
	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == TEXT("PinTags"))
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

FText UGameplayTagsK2Node_SwitchGameplayTag::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("K2Node", "Switch_Tag", "Switch on Gameplay Tag");
}

FText UGameplayTagsK2Node_SwitchGameplayTag::GetTooltipText() const
{
	return NSLOCTEXT("K2Node", "SwitchTag_ToolTip", "Selects an output that matches the input value");
}

void UGameplayTagsK2Node_SwitchGameplayTag::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
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

void UGameplayTagsK2Node_SwitchGameplayTag::CreateSelectionPin()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* Pin = CreatePin(EGPD_Input, K2Schema->PC_Struct, TEXT(""), FGameplayTag::StaticStruct(), false, false, TEXT("Selection"));
	K2Schema->SetPinDefaultValueBasedOnType(Pin);
}

FEdGraphPinType UGameplayTagsK2Node_SwitchGameplayTag::GetPinType() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	FEdGraphPinType PinType;
	PinType.PinCategory = K2Schema->PC_Struct;
	PinType.PinSubCategoryObject = FGameplayTag::StaticStruct();
	return PinType;
}

FString UGameplayTagsK2Node_SwitchGameplayTag::GetPinNameGivenIndex(int32 Index)
{
	check(Index);
	return PinNames[Index].ToString();
}

void UGameplayTagsK2Node_SwitchGameplayTag::CreateCasePins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	for (int32 Index = 0; Index < PinNames.Num(); ++Index)
  	{	
		if (Index < PinTags.Num())
		{
			if (PinTags[Index].IsValid())
			{
				PinNames[Index] = FName(*PinTags[Index].ToString());
			}			
			else
			{
				PinNames[Index] = FName(*GetUniquePinName());
			}
		}
		CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, PinNames[Index].ToString());
  	}
}

FString UGameplayTagsK2Node_SwitchGameplayTag::GetUniquePinName()
{
	FString NewPinName;
	int32 Index = 0;
	while (true)
	{
		NewPinName = FString::Printf(TEXT("Case_%d"), Index++);
		if (!FindPin(NewPinName))
		{
			break;
		}
	}
	return NewPinName;
}

void UGameplayTagsK2Node_SwitchGameplayTag::AddPinToSwitchNode()
{
	FString PinName = GetUniquePinName();
	PinNames.Add(FName(*PinName));

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* NewPin = CreatePin(EGPD_Output, K2Schema->PC_Exec, TEXT(""), NULL, false, false, PinName);
	if (PinTags.Num() < PinNames.Num())
	{ 
		PinTags.Add(FGameplayTag());
	}
}

void UGameplayTagsK2Node_SwitchGameplayTag::RemovePin(UEdGraphPin* TargetPin)
{
	checkSlow(TargetPin);

	FString TagName = TargetPin->PinName;

	int32 Index = PinNames.IndexOfByKey(FName(*TagName));
	if (Index>=0)
	{
		if (Index < PinTags.Num())
		{ 
			PinTags.RemoveAt(Index);
		}		
		PinNames.RemoveSingle(FName(*TagName));
	}	
}