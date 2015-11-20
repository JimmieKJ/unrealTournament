// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsEditorModulePrivatePCH.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "GameplayTagContainer.h"
#include "BlueprintGameplayTagLibrary.h"
#include "GameplayTagsModule.h"
#include "GameplayTagsK2Node_MultiCompareGameplayTagContainerSingleTags.h"
#include "KismetCompiler.h"

UGameplayTagsK2Node_MultiCompareGameplayTagContainerSingleTags::UGameplayTagsK2Node_MultiCompareGameplayTagContainerSingleTags(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UGameplayTagsK2Node_MultiCompareGameplayTagContainerSingleTags::AllocateDefaultPins()
{
	PinNames.Empty();
	for (int32 Index = 0; Index < NumberOfPins; ++Index)
	{
		AddPinToSwitchNode();
	}

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	CreatePin(EGPD_Input, K2Schema->PC_Struct, TEXT(""), FGameplayTagContainer::StaticStruct(), false, true, TEXT("Gameplay Tag Container"));
	UEnum* EnumClass = FindObject<UEnum>(NULL, TEXT("GameplayTags.EGameplayTagMatchType"));
	CreatePin(EGPD_Input, K2Schema->PC_Byte, TEXT(""), EnumClass, false, false, TEXT("Tag Container Match Type"));
	CreatePin(EGPD_Input, K2Schema->PC_Byte, TEXT(""), EnumClass, false, false, TEXT("Tags Match Type"));
}

void UGameplayTagsK2Node_MultiCompareGameplayTagContainerSingleTags::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Get The input and output pins to our node
	UEdGraphPin* InPinSwitch = FindPin(TEXT("Gameplay Tag Container"));
	UEdGraphPin* InPinContainerMatchType = FindPin(TEXT("Tag Container Match Type"));
	UEdGraphPin* InPinTagsMatchType = FindPin(TEXT("Tags Match Type"));

	// For Each Pin Compare against the Tag container
	for (int32 Index = 0; Index < NumberOfPins; ++Index)
	{
		FString InPinName = TEXT("TagCase_") + FString::FormatAsNumber(Index);
		FString OutPinName = TEXT("Case_") + FString::FormatAsNumber(Index) + TEXT(" True");
		UEdGraphPin* InPinCase = FindPin(InPinName);
		UEdGraphPin* OutPinCase = FindPin(OutPinName);

		// Create call function node for the Compare function HasAllMatchingGameplayTags
		UK2Node_CallFunction* PinCallFunction = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		const UFunction* Function = UBlueprintGameplayTagLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UBlueprintGameplayTagLibrary, DoesContainerHaveTag));
		PinCallFunction->SetFromFunction(Function);
		PinCallFunction->AllocateDefaultPins();

		UEdGraphPin *TagContainerPin = PinCallFunction->FindPinChecked(FString(TEXT("TagContainer")));
		CompilerContext.CopyPinLinksToIntermediate(*InPinSwitch, *TagContainerPin);

		UEdGraphPin *TagPin = PinCallFunction->FindPinChecked(FString(TEXT("Tag")));
		CompilerContext.MovePinLinksToIntermediate(*InPinCase, *TagPin);

		UEdGraphPin *ContainerTagsMatchTypePin = PinCallFunction->FindPinChecked(FString(TEXT("ContainerTagsMatchType")));
		CompilerContext.CopyPinLinksToIntermediate(*InPinContainerMatchType, *ContainerTagsMatchTypePin);

		UEdGraphPin *TagMatchTypePin = PinCallFunction->FindPinChecked(FString(TEXT("TagMatchType")));
		CompilerContext.CopyPinLinksToIntermediate(*InPinTagsMatchType, *TagMatchTypePin);
		
		UEdGraphPin *OutPin = PinCallFunction->FindPinChecked(K2Schema->PN_ReturnValue);

		if (OutPinCase && OutPin)
		{
			OutPin->PinType = OutPinCase->PinType; // Copy type so it uses the right actor subclass
			CompilerContext.MovePinLinksToIntermediate(*OutPinCase, *OutPin);
		}
	}

	// Break any links to the expanded node
	BreakAllNodeLinks();
}

FText UGameplayTagsK2Node_MultiCompareGameplayTagContainerSingleTags::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("K2Node", "MultiCompare_TagContainerSingleTags", "Compare Tag Container to Other Tags");
}

void UGameplayTagsK2Node_MultiCompareGameplayTagContainerSingleTags::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
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

void UGameplayTagsK2Node_MultiCompareGameplayTagContainerSingleTags::AddPinToSwitchNode()
{
	FString PinName = GetUniquePinName();
	FString InPin = TEXT("Tag") + PinName;
	FString OutPin = PinName + TEXT(" True");
	PinNames.Add(FName(*PinName));

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	CreatePin(EGPD_Input, K2Schema->PC_Struct, TEXT(""), FGameplayTag::StaticStruct(), false, true, InPin);
	CreatePin(EGPD_Output, K2Schema->PC_Boolean, TEXT(""), NULL, false, false, OutPin);
}